/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "encoder_port.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_timer.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#ifndef ENCODER_ESP32_GPIO_A
#define ENCODER_ESP32_GPIO_A        (4) /* A 相 GPIO */
#endif

#ifndef ENCODER_ESP32_GPIO_B
#define ENCODER_ESP32_GPIO_B        (5) /* B 相 GPIO */
#endif

#ifndef ENCODER_ESP32_GPIO_INDEX
#define ENCODER_ESP32_GPIO_INDEX    (-1) /* Z 相 GPIO，负值表示不用 */
#endif

#ifndef ENCODER_ESP32_USE_PULLUP
#define ENCODER_ESP32_USE_PULLUP    (1) /* 1: 使用内部上拉 */
#endif

#ifndef ENCODER_ESP32_GLITCH_NS
#define ENCODER_ESP32_GLITCH_NS     (1000) /* 输入毛刺滤波（ns），0 表示关闭 */
#endif

#define ENCODER_ESP32_HIGH_LIMIT    (32767)  /* PCNT 高位限值 */
#define ENCODER_ESP32_LOW_LIMIT     (-32768) /* PCNT 低位限值 */

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/
typedef struct
{
    pcnt_unit_handle_t    unit;        /* PCNT 单元 */
    pcnt_channel_handle_t chan_a;      /* A 相通道 */
    pcnt_channel_handle_t chan_b;      /* B 相通道 */
    int32_t               last_count;  /* 上次读到的累计计数 */
    volatile uint32_t     index_count; /* Z 脉冲累计数 */
    bool                  index_handler_added; /* Z 中断是否已注册 */
    bool                  initialized;          /* port 是否已初始化 */
} encoder_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static int      esp32_encoder_init(void);
static int32_t  esp32_encoder_read_count(void);
static uint64_t esp32_encoder_get_time_us(void);
#if ENCODER_ESP32_GPIO_INDEX >= 0
static uint32_t esp32_encoder_get_index_count(void);
static void     esp32_encoder_index_isr(void *arg);
#endif
static int      esp32_encoder_deinit(void);
static void     esp32_encoder_release(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static encoder_esp32_ctx_t g_encoder_ctx; /* port 运行状态 */

static const encoder_port_driver_t g_encoder_port_driver = {
    .init = esp32_encoder_init,                 /* 初始化硬件 */
    .read_count = esp32_encoder_read_count,     /* 读取累计计数 */
    .get_time_us = esp32_encoder_get_time_us,   /* 读取单调时间 */
#if ENCODER_ESP32_GPIO_INDEX >= 0
    .get_index_count = esp32_encoder_get_index_count, /* 读取 Z 计数 */
#else
    .get_index_count = NULL,                    /* 未配置 Z 相 */
#endif
    .deinit = esp32_encoder_deinit,             /* 释放硬件 */
};

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief 获取编码器 port 驱动实例
 *
 * @return 驱动实例指针
 */
const encoder_port_driver_t *encoder_port_get_driver(void)
{
    return &g_encoder_port_driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief 释放 PCNT 和 GPIO 资源
 */
static void esp32_encoder_release(void)
{
#if ENCODER_ESP32_GPIO_INDEX >= 0
    /* 先移除 Z 中断，避免释放过程中继续计数 */
    if (g_encoder_ctx.index_handler_added)
    {
        gpio_isr_handler_remove(ENCODER_ESP32_GPIO_INDEX);
        g_encoder_ctx.index_handler_added = false;
    }
#endif

    if (g_encoder_ctx.unit != NULL)
    {
        /* 先停止计数，才能删除通道和单元 */
        pcnt_unit_stop(g_encoder_ctx.unit);
        pcnt_unit_disable(g_encoder_ctx.unit);
    }

    /* 先删通道，再删 PCNT 单元 */
    if (g_encoder_ctx.chan_a != NULL)
    {
        pcnt_del_channel(g_encoder_ctx.chan_a);
        g_encoder_ctx.chan_a = NULL;
    }

    if (g_encoder_ctx.chan_b != NULL)
    {
        pcnt_del_channel(g_encoder_ctx.chan_b);
        g_encoder_ctx.chan_b = NULL;
    }

    if (g_encoder_ctx.unit != NULL)
    {
        pcnt_del_unit(g_encoder_ctx.unit);
        g_encoder_ctx.unit = NULL;
    }

    /* 清空上下文，允许下次重新初始化 */
    memset(&g_encoder_ctx, 0, sizeof(g_encoder_ctx));
}

/**
 * @brief 初始化 ESP32 PCNT 正交编码器
 *
 * @return 0: 成功, -1: 失败
 */
static int esp32_encoder_init(void)
{
    pcnt_unit_config_t unit_config = {
        .high_limit = ENCODER_ESP32_HIGH_LIMIT,
        .low_limit = ENCODER_ESP32_LOW_LIMIT,
        .flags.accum_count = true, /* 启用 16 位溢出补偿 */
    };
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENCODER_ESP32_GPIO_A,
        .level_gpio_num = ENCODER_ESP32_GPIO_B,
    };
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = ENCODER_ESP32_GPIO_B,
        .level_gpio_num = ENCODER_ESP32_GPIO_A,
    };
    esp_err_t ret;

    /* 重复初始化直接返回 */
    if (g_encoder_ctx.initialized)
    {
        return 0;
    }

    memset(&g_encoder_ctx, 0, sizeof(g_encoder_ctx));

    /* 分配 PCNT 单元 */
    ret = pcnt_new_unit(&unit_config, &g_encoder_ctx.unit);
    if (ret != ESP_OK)
    {
        goto err;
    }

    /* A 相：A 边沿计数，B 电平决定方向 */
    ret = pcnt_new_channel(g_encoder_ctx.unit, &chan_a_config, &g_encoder_ctx.chan_a);
    if (ret != ESP_OK)
    {
        goto err;
    }

    /* B 相：B 边沿计数，A 电平决定方向 */
    ret = pcnt_new_channel(g_encoder_ctx.unit, &chan_b_config, &g_encoder_ctx.chan_b);
    if (ret != ESP_OK)
    {
        goto err;
    }

    /* 两通道组合成 x4 正交解码 */
    ret = pcnt_channel_set_edge_action(g_encoder_ctx.chan_a,
                                       PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                       PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    if (ret != ESP_OK)
    {
        goto err;
    }

    ret = pcnt_channel_set_level_action(g_encoder_ctx.chan_a,
                                        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    if (ret != ESP_OK)
    {
        goto err;
    }

    ret = pcnt_channel_set_edge_action(g_encoder_ctx.chan_b,
                                       PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                       PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    if (ret != ESP_OK)
    {
        goto err;
    }

    ret = pcnt_channel_set_level_action(g_encoder_ctx.chan_b,
                                        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    if (ret != ESP_OK)
    {
        goto err;
    }

#if ENCODER_ESP32_GLITCH_NS > 0
    /* 配置输入毛刺滤波 */
    {
        pcnt_glitch_filter_config_t filter_config = {
            .max_glitch_ns = ENCODER_ESP32_GLITCH_NS,
        };

        ret = pcnt_unit_set_glitch_filter(g_encoder_ctx.unit, &filter_config);
        if (ret != ESP_OK)
        {
            goto err;
        }
    }
#endif

    /* 高低限 watch point 是 accum_count 溢出补偿的必要条件 */
    ret = pcnt_unit_add_watch_point(g_encoder_ctx.unit, ENCODER_ESP32_HIGH_LIMIT);
    if (ret != ESP_OK)
    {
        goto err;
    }

    ret = pcnt_unit_add_watch_point(g_encoder_ctx.unit, ENCODER_ESP32_LOW_LIMIT);
    if (ret != ESP_OK)
    {
        goto err;
    }

    /* 配置 A/B 上拉 */
    gpio_set_pull_mode(ENCODER_ESP32_GPIO_A,
                       ENCODER_ESP32_USE_PULLUP ? GPIO_PULLUP_ONLY : GPIO_FLOATING);
    gpio_set_pull_mode(ENCODER_ESP32_GPIO_B,
                       ENCODER_ESP32_USE_PULLUP ? GPIO_PULLUP_ONLY : GPIO_FLOATING);

#if ENCODER_ESP32_GPIO_INDEX >= 0
    /* 配置 Z 相上升沿中断 */
    {
        gpio_config_t index_config = {
            .pin_bit_mask = (1ULL << ENCODER_ESP32_GPIO_INDEX),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = ENCODER_ESP32_USE_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_POSEDGE,
        };

        ret = gpio_config(&index_config);
        if (ret != ESP_OK)
        {
            goto err;
        }

        /* 已安装过 ISR 服务时忽略重复安装错误 */
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        {
            goto err;
        }

        ret = gpio_isr_handler_add(ENCODER_ESP32_GPIO_INDEX, esp32_encoder_index_isr, NULL);
        if (ret != ESP_OK)
        {
            goto err;
        }

        g_encoder_ctx.index_handler_added = true;
    }
#endif

    /* 使能并启动 PCNT */
    ret = pcnt_unit_enable(g_encoder_ctx.unit);
    if (ret != ESP_OK)
    {
        goto err;
    }

    ret = pcnt_unit_clear_count(g_encoder_ctx.unit);
    if (ret != ESP_OK)
    {
        goto err;
    }

    ret = pcnt_unit_start(g_encoder_ctx.unit);
    if (ret != ESP_OK)
    {
        goto err;
    }

    /* 清零计数和 Z 计数，建立差分基准 */
    g_encoder_ctx.last_count = 0;
    g_encoder_ctx.index_count = 0;
    g_encoder_ctx.initialized = true;
    return 0;

err:
    /* 任一步失败，释放已申请资源 */
    esp32_encoder_release();
    return -1;
}

/**
 * @brief 读取累计计数
 *
 * @return 累计计数
 */
static int32_t esp32_encoder_read_count(void)
{
    int count = 0;

    /* 未初始化时返回 0 */
    if (!g_encoder_ctx.initialized || g_encoder_ctx.unit == NULL)
    {
        return 0;
    }

    /* 读取失败时保留上次值 */
    if (pcnt_unit_get_count(g_encoder_ctx.unit, &count) == ESP_OK)
    {
        g_encoder_ctx.last_count = (int32_t)count;
    }

    return g_encoder_ctx.last_count;
}

/**
 * @brief 读取单调微秒时间
 *
 * @return 微秒时间
 */
static uint64_t esp32_encoder_get_time_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

#if ENCODER_ESP32_GPIO_INDEX >= 0
/**
 * @brief 读取 Z 脉冲累计数
 *
 * @return Z 脉冲累计数
 */
static uint32_t esp32_encoder_get_index_count(void)
{
    return g_encoder_ctx.index_count;
}

/**
 * @brief Z 相 GPIO 中断处理
 *
 * @param arg 未使用
 */
static void IRAM_ATTR esp32_encoder_index_isr(void *arg)
{
    (void)arg;

    /* 只累计脉冲数，吸附/清零由 core 处理 */
    g_encoder_ctx.index_count++;
}
#endif

/**
 * @brief 反初始化 ESP32 PCNT 正交编码器
 *
 * @return 0: 成功
 */
static int esp32_encoder_deinit(void)
{
    esp32_encoder_release();
    return 0;
}
