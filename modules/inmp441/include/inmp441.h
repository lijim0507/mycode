#ifndef __INMP441_H_
#define __INMP441_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>

#include "inmp441_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  INMP441 输出通道
 */
typedef enum
{
    INMP441_CHANNEL_LEFT  = 0,
    INMP441_CHANNEL_RIGHT = 1,
} inmp441_channel_t;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/*
 * 返回值约定：
 *    0: 成功
 *   -1: 参数错误或模块未初始化
 *   -2: port 驱动无效
 *   -3: I2S 硬件初始化失败
 *   -4: I2S 读取失败或超时
 */

/**
 * @brief  初始化 INMP441 模块
 *
 * 内部调用 inmp441_port_get_driver() 获取平台 I2S 驱动。采样率与通道由
 * inmp441_config.h 配置，I2S 端口和引脚由 port 层文件内部配置。
 *
 * @return 0: 成功, -2: port 驱动无效, -3: I2S 硬件初始化失败
 * @note   模块为单实例，重复调用会先反初始化。
 */
int inmp441_init(void);

/**
 * @brief  反初始化 INMP441 模块
 *
 * @return 0: 成功
 */
int inmp441_deinit(void);

/**
 * @brief  读取 24bit 原始采样
 *
 * @param  samples      输出缓冲区
 * @param  sample_count 期望读取的采样数
 * @param  timeout_ms   读取超时（ms），0 表示不等待
 * @param  samples_read 实际读取的采样数，可为 NULL
 * @return 0: 成功, -1: 参数错误或模块未初始化, -4: I2S 读取失败或超时
 * @note   每个采样为 INMP441 的 24bit 补码值，符号扩展至 int32，
 *         范围 -8388608 ~ 8388607。超时时 samples_read 可能小于 sample_count。
 */
int inmp441_read(int32_t *samples, uint32_t sample_count, uint32_t timeout_ms,
                 uint32_t *samples_read);

/**
 * @brief  读取 16bit PCM 采样
 *
 * @param  samples      输出缓冲区
 * @param  sample_count 期望读取的采样数
 * @param  timeout_ms   单次底层读取的超时时间（ms），0 表示不等待
 * @param  samples_read 实际读取的采样数，可为 NULL
 * @return 0: 成功, -1: 参数错误或模块未初始化, -4: I2S 读取失败或超时
 * @note   内部按 INMP441_CONV_CHUNK_SAMPLES 分块读取，将 24bit 数据右移 8 位
 *         转换为 16bit。超时时 samples_read 可能小于 sample_count。
 *         内部使用静态转换缓冲，不支持多任务并发调用。
 */
int inmp441_read_int16(int16_t *samples, uint32_t sample_count, uint32_t timeout_ms,
                       uint32_t *samples_read);

/**
 * @brief  将 24bit 原始采样转换为 16bit PCM
 *
 * @param  raw          原始采样缓冲区，来自 inmp441_read()
 * @param  pcm          输出缓冲区
 * @param  sample_count 采样数
 * @note   输入缓冲区与输出缓冲区不能重叠。
 */
void inmp441_raw_to_int16(const int32_t *raw, int16_t *pcm, uint32_t sample_count);

/**
 * @brief  获取当前配置的采样率
 *
 * @return 采样率（Hz）
 */
uint32_t inmp441_get_sample_rate(void);

/**
 * @brief  获取当前配置的麦克风通道
 *
 * @return INMP441_CHANNEL_LEFT: 左声道, INMP441_CHANNEL_RIGHT: 右声道
 */
inmp441_channel_t inmp441_get_channel(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/