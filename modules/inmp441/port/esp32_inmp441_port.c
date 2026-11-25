/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "inmp441_port.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* 接线：SCK -> BCLK, WS -> WS, SD -> DIN；L/R 引脚由 INMP441_CHANNEL_SELECT 决定 */
#ifndef INMP441_ESP32_I2S_PORT
#define INMP441_ESP32_I2S_PORT          I2S_NUM_0
#endif

#ifndef INMP441_ESP32_SCK_GPIO
#define INMP441_ESP32_SCK_GPIO          GPIO_NUM_14
#endif

#ifndef INMP441_ESP32_WS_GPIO
#define INMP441_ESP32_WS_GPIO           GPIO_NUM_15
#endif

#ifndef INMP441_ESP32_SD_GPIO
#define INMP441_ESP32_SD_GPIO           GPIO_NUM_32
#endif

/* DMA 描述符数量，建议不小于 2 */
#ifndef INMP441_ESP32_DMA_DESC_NUM
#define INMP441_ESP32_DMA_DESC_NUM      4
#endif

/* 每个 DMA 描述符的帧数，需为 4 的倍数 */
#ifndef INMP441_ESP32_DMA_FRAME_NUM
#define INMP441_ESP32_DMA_FRAME_NUM     240
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef struct
{
    i2s_chan_handle_t rx_handle;
    bool              initialized;
} inmp441_esp32_ctx_t;

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  esp32_inmp441_init(void);
static int  esp32_inmp441_deinit(void);
static int  esp32_inmp441_read(int32_t *samples, uint32_t sample_count,
                               uint32_t timeout_ms, uint32_t *samples_read);
static void esp32_inmp441_cleanup(void);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static inmp441_esp32_ctx_t g_ctx;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 ESP32 INMP441 驱动实例
 *
 * @return 驱动实例指针
 */
const inmp441_port_driver_t *inmp441_port_get_driver(void)
{
    static const inmp441_port_driver_t driver = {
        .init   = esp32_inmp441_init,
        .deinit = esp32_inmp441_deinit,
        .read   = esp32_inmp441_read,
    };

    return &driver;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 ESP32 I2S 接收通道
 *
 * @return 0: 成功, -1: 通道创建失败, -2: 标准模式配置失败, -3: 通道使能失败
 */
static int esp32_inmp441_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(INMP441_ESP32_I2S_PORT, I2S_ROLE_MASTER);
    i2s_std_config_t  std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(INMP441_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = INMP441_ESP32_SCK_GPIO,
            .ws   = INMP441_ESP32_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din  = INMP441_ESP32_SD_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    esp_err_t ret;

    if (g_ctx.initialized)
    {
        return 0;
    }

    chan_cfg.dma_desc_num  = INMP441_ESP32_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = INMP441_ESP32_DMA_FRAME_NUM;

#if INMP441_CHANNEL_SELECT == 0
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
#elif INMP441_CHANNEL_SELECT == 1
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
#else
#error "INMP441_CHANNEL_SELECT must be 0 (left) or 1 (right)"
#endif

    ret = i2s_new_channel(&chan_cfg, NULL, &g_ctx.rx_handle);
    if (ret != ESP_OK)
    {
        g_ctx.rx_handle = NULL;
        return -1;
    }

    ret = i2s_channel_init_std_mode(g_ctx.rx_handle, &std_cfg);
    if (ret != ESP_OK)
    {
        esp32_inmp441_cleanup();
        return -2;
    }

    ret = i2s_channel_enable(g_ctx.rx_handle);
    if (ret != ESP_OK)
    {
        esp32_inmp441_cleanup();
        return -3;
    }

    g_ctx.initialized = true;
    return 0;
}

/**
 * @brief  反初始化 ESP32 I2S 接收通道
 *
 * @return 0: 成功
 */
static int esp32_inmp441_deinit(void)
{
    esp32_inmp441_cleanup();
    return 0;
}

/**
 * @brief  读取 I2S 原始数据
 *
 * @param  samples      输出缓冲区
 * @param  sample_count 期望读取的采样数
 * @param  timeout_ms   读取超时（ms）
 * @param  samples_read 实际读取的采样数，可为 NULL
 * @return 0: 成功, -1: 参数错误或未初始化, -2: I2S 读取失败
 */
static int esp32_inmp441_read(int32_t *samples, uint32_t sample_count,
                              uint32_t timeout_ms, uint32_t *samples_read)
{
    size_t    bytes_read = 0;
    esp_err_t ret;

    if (samples_read != NULL)
    {
        *samples_read = 0;
    }

    if (!g_ctx.initialized || samples == NULL || sample_count == 0)
    {
        return -1;
    }

    if (sample_count > (SIZE_MAX / sizeof(int32_t)))
    {
        return -1;
    }

    ret = i2s_channel_read(g_ctx.rx_handle, samples,
                           (size_t)sample_count * sizeof(int32_t),
                           &bytes_read, timeout_ms);
    if (ret != ESP_OK)
    {
        return -2;
    }

    if (samples_read != NULL)
    {
        *samples_read = (uint32_t)(bytes_read / sizeof(int32_t));
    }

    return 0;
}

/**
 * @brief  释放 I2S 通道资源
 */
static void esp32_inmp441_cleanup(void)
{
    if (g_ctx.rx_handle != NULL)
    {
        i2s_channel_disable(g_ctx.rx_handle);
        i2s_channel_deinit(g_ctx.rx_handle);
        g_ctx.rx_handle = NULL;
    }

    g_ctx.initialized = false;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/