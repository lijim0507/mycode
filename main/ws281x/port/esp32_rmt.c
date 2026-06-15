/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws281x_port.h"

#include <string.h>

#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#ifndef WS281X_ESP32_GPIO_NUM
#define WS281X_ESP32_GPIO_NUM       GPIO_NUM_2
#endif

/* RMT 时钟分辨率：10MHz → 100ns/tick */
#define WS281X_RMT_RESOLUTION_HZ  (10 * 1000 * 1000)

/*
 * WS281X 时序参数 (@10MHz → 100ns/tick):
 *   T0H: 220~380 ns  → 4 ticks (400 ns)
 *   T0L: 580~1000 ns → 8 ticks (800 ns)
 *   T1H: 580~1000 ns → 7 ticks (700 ns)
 *   T1L: 220~420 ns  → 6 ticks (600 ns)
 *
 * 以上数值均在 WS2812B 规格范围内，实际使用示波器验证。
 */
#ifndef WS281X_RMT_T0H_TICKS
#define WS281X_RMT_T0H_TICKS  4
#endif

#ifndef WS281X_RMT_T0L_TICKS
#define WS281X_RMT_T0L_TICKS  8
#endif

#ifndef WS281X_RMT_T1H_TICKS
#define WS281X_RMT_T1H_TICKS  7
#endif

#ifndef WS281X_RMT_T1L_TICKS
#define WS281X_RMT_T1L_TICKS  6
#endif

/*
 * RESET 信号：>50us 低电平。
 * 通过 rmt_transmit_config_t::eot_level = 0 实现，
 * 传输结束后自动保持低电平，不浪费 RMT 内存空间。
 */

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/**
 * @brief  ESP32 RMT 驱动上下文
 */
typedef struct {
    rmt_channel_handle_t   tx_channel;
    rmt_encoder_handle_t   bytes_encoder;
    gpio_num_t             gpio_num;
    SemaphoreHandle_t      tx_sem;
    bool                   busy;
} esp32_rmt_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static bool IRAM_ATTR rmt_tx_done_cb(rmt_channel_handle_t tx_chan,
                                      const rmt_tx_done_event_data_t *edata,
                                      void *user_ctx);

static int esp32_rmt_init(void);
static int esp32_rmt_transmit(const uint8_t *data, uint32_t len);
static int esp32_rmt_is_busy(void);
static int esp32_rmt_deinit(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static esp32_rmt_ctx_t g_rmt_ctx;

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  RMT 发送完成中断回调
 * @param  tx_chan  完成的 RMT TX 通道
 * @param  edata    事件数据
 * @param  user_ctx 用户上下文（未使用）
 * @return 是否需要触发任务切换
 */
static bool IRAM_ATTR rmt_tx_done_cb(rmt_channel_handle_t tx_chan,
                                      const rmt_tx_done_event_data_t *edata,
                                      void *user_ctx)
{
    (void)tx_chan;
    (void)edata;
    (void)user_ctx;
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(g_rmt_ctx.tx_sem, &high_task_wakeup);
    g_rmt_ctx.busy = false;
    return (high_task_wakeup == pdTRUE);
}

/**
 * @brief  ESP32 RMT 硬件初始化
 * @return 0: 成功, -1: 信号量创建失败, -2: RMT 通道创建失败,
 *         -3: 编码器创建失败, -4: RMT 使能失败
 * @note   GPIO 通过 WS281X_ESP32_GPIO_NUM 宏定义指定
 */
static int esp32_rmt_init(void)
{
    gpio_num_t gpio_num = WS281X_ESP32_GPIO_NUM;

    g_rmt_ctx.gpio_num = gpio_num;

    g_rmt_ctx.tx_sem = xSemaphoreCreateBinary();
    if (!g_rmt_ctx.tx_sem)
    {
        return -1;
    }

    rmt_tx_channel_config_t tx_chan_cfg = {
        .gpio_num = gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = WS281X_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.with_dma = false,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_chan_cfg, &g_rmt_ctx.tx_channel);
    if (ret != ESP_OK)
    {
        vSemaphoreDelete(g_rmt_ctx.tx_sem);
        return -2;
    }

    rmt_tx_event_callbacks_t cbs = {
        .on_trans_done = rmt_tx_done_cb,
    };
    rmt_tx_register_event_callbacks(g_rmt_ctx.tx_channel, &cbs, NULL);

    rmt_bytes_encoder_config_t bytes_enc_cfg = {
        .bit0 = {
            .duration0 = WS281X_RMT_T0H_TICKS,
            .level0    = 1,
            .duration1 = WS281X_RMT_T0L_TICKS,
            .level1    = 0,
        },
        .bit1 = {
            .duration0 = WS281X_RMT_T1H_TICKS,
            .level0    = 1,
            .duration1 = WS281X_RMT_T1L_TICKS,
            .level1    = 0,
        },
        .flags.msb_first = 1,
    };
    ret = rmt_new_bytes_encoder(&bytes_enc_cfg, &g_rmt_ctx.bytes_encoder);
    if (ret != ESP_OK)
    {
        rmt_del_channel(g_rmt_ctx.tx_channel);
        vSemaphoreDelete(g_rmt_ctx.tx_sem);
        return -3;
    }

    ret = rmt_enable(g_rmt_ctx.tx_channel);
    if (ret != ESP_OK)
    {
        rmt_del_encoder(g_rmt_ctx.bytes_encoder);
        rmt_del_channel(g_rmt_ctx.tx_channel);
        vSemaphoreDelete(g_rmt_ctx.tx_sem);
        return -4;
    }

    g_rmt_ctx.busy = false;
    return 0;
}

/**
 * @brief  通过 RMT 发送已编码的 WS281X 数据
 * @param  data 编码后的发送缓冲区（GRB 字节流）
 * @param  len  数据长度（字节数）
 * @return 0: 成功, -1: 发送启动失败
 * @note   eot_level = 0 确保传输结束后自动产生 RESET 信号
 */
static int esp32_rmt_transmit(const uint8_t *data, uint32_t len)
{
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags.eot_level = 0,
    };

    g_rmt_ctx.busy = true;
    esp_err_t ret = rmt_transmit(g_rmt_ctx.tx_channel,
                                  g_rmt_ctx.bytes_encoder,
                                  data, len, &tx_cfg);
    if (ret != ESP_OK)
    {
        g_rmt_ctx.busy = false;
        return -1;
    }
    return 0;
}

/**
 * @brief  查询 RMT 发送是否完成
 * @return 1: 正在发送, 0: 空闲
 */
static int esp32_rmt_is_busy(void)
{
    if (g_rmt_ctx.busy)
    {
        if (xSemaphoreTake(g_rmt_ctx.tx_sem, 0) == pdTRUE)
        {
            g_rmt_ctx.busy = false;
        }
    }
    return g_rmt_ctx.busy ? 1 : 0;
}

/**
 * @brief  反初始化 RMT 硬件，释放所有资源
 * @return 0: 成功
 */
static int esp32_rmt_deinit(void)
{
    if (g_rmt_ctx.tx_channel)
    {
        rmt_disable(g_rmt_ctx.tx_channel);
        rmt_del_encoder(g_rmt_ctx.bytes_encoder);
        rmt_del_channel(g_rmt_ctx.tx_channel);
    }
    if (g_rmt_ctx.tx_sem)
    {
        vSemaphoreDelete(g_rmt_ctx.tx_sem);
    }
    memset(&g_rmt_ctx, 0, sizeof(g_rmt_ctx));
    return 0;
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  获取 ESP32 RMT 驱动实例
 * @return ws281x_driver_t 结构体指针
 */
const ws281x_driver_t *ws281x_port_get_driver(void)
{
    static const ws281x_driver_t driver = {
        .init      = esp32_rmt_init,
        .transmit  = esp32_rmt_transmit,
        .is_busy   = esp32_rmt_is_busy,
        .deinit    = esp32_rmt_deinit,
    };
    return &driver;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/