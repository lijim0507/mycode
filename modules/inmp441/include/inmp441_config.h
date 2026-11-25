#ifndef __INMP441_CONFIG_H_
#define __INMP441_CONFIG_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* I2S 采样率（Hz），INMP441 常用 16000，具体范围参考数据手册 */
#ifndef INMP441_SAMPLE_RATE_HZ
#define INMP441_SAMPLE_RATE_HZ          16000U
#endif

/* 麦克风通道：0: 左声道（L/R 接 GND）, 1: 右声道（L/R 接 VDD） */
#ifndef INMP441_CHANNEL_SELECT
#define INMP441_CHANNEL_SELECT          0
#endif

/* 默认读取超时（ms），供调用方作为 inmp441_read() 的 timeout_ms 参数 */
#ifndef INMP441_READ_TIMEOUT_MS
#define INMP441_READ_TIMEOUT_MS         1000U
#endif

/* 16bit 转换时的内部缓冲深度（采样点），必须大于 0 */
#ifndef INMP441_CONV_CHUNK_SAMPLES
#define INMP441_CONV_CHUNK_SAMPLES      256U
#endif

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/