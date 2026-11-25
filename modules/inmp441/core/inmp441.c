/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "inmp441.h"
#include "inmp441_port.h"

#include <stdbool.h>
#include <stddef.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#if INMP441_SAMPLE_RATE_HZ == 0
#error "INMP441_SAMPLE_RATE_HZ must be greater than 0"
#endif

#if (INMP441_CHANNEL_SELECT != 0) && (INMP441_CHANNEL_SELECT != 1)
#error "INMP441_CHANNEL_SELECT must be 0 (left) or 1 (right)"
#endif

#if INMP441_CONV_CHUNK_SAMPLES == 0
#error "INMP441_CONV_CHUNK_SAMPLES must be greater than 0"
#endif

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const inmp441_port_driver_t *g_port;
static bool                         g_initialized;
static int32_t                      g_conv_buf[INMP441_CONV_CHUNK_SAMPLES];

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 INMP441 模块
 *
 * @return 0: 成功, -2: port 驱动无效, -3: I2S 硬件初始化失败
 */
int inmp441_init(void)
{
    const inmp441_port_driver_t *driver;

    driver = inmp441_port_get_driver();
    if (driver == NULL || driver->init == NULL || driver->read == NULL)
    {
        return -2;
    }

    if (g_initialized)
    {
        inmp441_deinit();
    }

    if (driver->init() != 0)
    {
        return -3;
    }

    g_port = driver;
    g_initialized = true;
    return 0;
}

/**
 * @brief  反初始化 INMP441 模块
 *
 * @return 0: 成功
 */
int inmp441_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    if (g_port != NULL && g_port->deinit != NULL)
    {
        g_port->deinit();
    }

    g_port = NULL;
    g_initialized = false;
    return 0;
}

/**
 * @brief  读取 24bit 原始采样
 *
 * @param  samples      输出缓冲区
 * @param  sample_count 期望读取的采样数
 * @param  timeout_ms   读取超时（ms）
 * @param  samples_read 实际读取的采样数，可为 NULL
 * @return 0: 成功, -1: 参数错误或未初始化, -4: I2S 读取失败或超时
 */
int inmp441_read(int32_t *samples, uint32_t sample_count, uint32_t timeout_ms,
                 uint32_t *samples_read)
{
    uint32_t count = 0;
    int      ret;

    if (samples_read != NULL)
    {
        *samples_read = 0;
    }

    if (!g_initialized || samples == NULL || sample_count == 0)
    {
        return -1;
    }

    ret = g_port->read(samples, sample_count, timeout_ms, &count);
    if (ret != 0)
    {
        return -4;
    }

    if (count > sample_count)
    {
        count = sample_count;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        samples[i] >>= 8;
    }

    if (samples_read != NULL)
    {
        *samples_read = count;
    }

    return 0;
}

/**
 * @brief  读取 16bit PCM 采样
 *
 * @param  samples      输出缓冲区
 * @param  sample_count 期望读取的采样数
 * @param  timeout_ms   单次底层读取的超时时间（ms）
 * @param  samples_read 实际读取的采样数，可为 NULL
 * @return 0: 成功, -1: 参数错误或未初始化, -4: I2S 读取失败或超时
 */
int inmp441_read_int16(int16_t *samples, uint32_t sample_count, uint32_t timeout_ms,
                       uint32_t *samples_read)
{
    uint32_t total = 0;

    if (samples_read != NULL)
    {
        *samples_read = 0;
    }

    if (!g_initialized || samples == NULL || sample_count == 0)
    {
        return -1;
    }

    while (total < sample_count)
    {
        uint32_t chunk = sample_count - total;
        uint32_t count = 0;
        int      ret;

        if (chunk > INMP441_CONV_CHUNK_SAMPLES)
        {
            chunk = INMP441_CONV_CHUNK_SAMPLES;
        }

        ret = inmp441_read(g_conv_buf, chunk, timeout_ms, &count);
        if (ret != 0)
        {
            if (samples_read != NULL)
            {
                *samples_read = total;
            }
            return ret;
        }

        inmp441_raw_to_int16(g_conv_buf, &samples[total], count);
        total += count;

        if (count == 0)
        {
            break;
        }
    }

    if (samples_read != NULL)
    {
        *samples_read = total;
    }

    return 0;
}

/**
 * @brief  将 24bit 原始采样转换为 16bit PCM
 *
 * @param  raw          原始采样缓冲区
 * @param  pcm          输出缓冲区
 * @param  sample_count 采样数
 */
void inmp441_raw_to_int16(const int32_t *raw, int16_t *pcm, uint32_t sample_count)
{
    if (raw == NULL || pcm == NULL)
    {
        return;
    }

    for (uint32_t i = 0; i < sample_count; i++)
    {
        pcm[i] = (int16_t)(raw[i] >> 8);
    }
}

/**
 * @brief  获取当前配置的采样率
 *
 * @return 采样率（Hz）
 */
uint32_t inmp441_get_sample_rate(void)
{
    return INMP441_SAMPLE_RATE_HZ;
}

/**
 * @brief  获取当前配置的麦克风通道
 *
 * @return INMP441_CHANNEL_LEFT: 左声道, INMP441_CHANNEL_RIGHT: 右声道
 */
inmp441_channel_t inmp441_get_channel(void)
{
#if INMP441_CHANNEL_SELECT == 1
    return INMP441_CHANNEL_RIGHT;
#else
    return INMP441_CHANNEL_LEFT;
#endif
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/