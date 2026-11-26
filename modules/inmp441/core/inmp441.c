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

/* 编译期配置检查 */
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

static const inmp441_port_driver_t *g_port;        /* 当前平台 I2S 驱动 */
static bool                         g_initialized; /* 模块初始化标志 */

/* 24bit 原始采样转换缓冲，默认 1KB，放在 BSS 不占任务栈 */
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

    /* 校验 port 是否实现必需接口 */
    driver = inmp441_port_get_driver();
    if (driver == NULL || driver->init == NULL || driver->read == NULL)
    {
        return -2;
    }

    /* 重复初始化时先释放旧资源 */
    if (g_initialized)
    {
        inmp441_deinit();
    }

    /* I2S 通道创建或配置失败 */
    if (driver->init() != 0)
    {
        return -3;
    }

    /* 保存驱动并标记初始化完成 */
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
    /* 未初始化时无需释放 */
    if (!g_initialized)
    {
        return 0;
    }

    /* 释放 port 层 I2S 资源 */
    if (g_port != NULL && g_port->deinit != NULL)
    {
        g_port->deinit();
    }

    /* 清空状态，允许重新初始化 */
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

    /* 先清零，失败时调用方也能拿到实际读取数 0 */
    if (samples_read != NULL)
    {
        *samples_read = 0;
    }

    /* 参数与初始化状态检查 */
    if (!g_initialized || samples == NULL || sample_count == 0)
    {
        return -1;
    }

    /* 从 port 读取 32bit slot 原始数据 */
    ret = g_port->read(samples, sample_count, timeout_ms, &count);
    if (ret != 0)
    {
        /* port 读取失败或超时 */
        return -4;
    }

    /* 防御异常返回，避免越界访问 */
    if (count > sample_count)
    {
        count = sample_count;
    }

    /* 24bit 数据左对齐在 32bit slot 高 24 位，右移 8 位得到原始值 */
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

    /* 先清零，失败时调用方也能拿到已转换数量 */
    if (samples_read != NULL)
    {
        *samples_read = 0;
    }

    /* 参数与初始化状态检查 */
    if (!g_initialized || samples == NULL || sample_count == 0)
    {
        return -1;
    }

    /* 按内部缓冲大小分块读取，直到填满或底层无数据 */
    while (total < sample_count)
    {
        uint32_t chunk = sample_count - total;
        uint32_t count = 0;
        int      ret;

        /* 单次读取不能超过转换缓冲容量 */
        if (chunk > INMP441_CONV_CHUNK_SAMPLES)
        {
            chunk = INMP441_CONV_CHUNK_SAMPLES;
        }

        /* 先读 24bit 原始数据到内部缓冲 */
        ret = inmp441_read(g_conv_buf, chunk, timeout_ms, &count);
        if (ret != 0)
        {
            /* 底层失败或超时，返回已转换的数量 */
            if (samples_read != NULL)
            {
                *samples_read = total;
            }
            return ret;
        }

        /* 24bit 转 16bit，并追加到调用方缓冲 */
        inmp441_raw_to_int16(g_conv_buf, &samples[total], count);
        total += count;

        /* 没有读到新数据时退出，避免死循环 */
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
    /* 空指针保护 */
    if (raw == NULL || pcm == NULL)
    {
        return;
    }

    /* 24bit 转 16bit，丢弃低 8 位 */
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
    /* 配置为右声道 */
    return INMP441_CHANNEL_RIGHT;
#else
    /* 默认左声道 */
    return INMP441_CHANNEL_LEFT;
#endif
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/