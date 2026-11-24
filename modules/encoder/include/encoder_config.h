#ifndef __ENCODER_CONFIG_H_
#define __ENCODER_CONFIG_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* 计数/圈（按 port 实际倍频，4 倍频为 PPR*4），必须大于 0，按实际编码器修改；
 * STM32 port 用它设置 ARR = CPR - 1，使一圈产生一次更新事件 */
#ifndef ENCODER_COUNTS_PER_REV
#define ENCODER_COUNTS_PER_REV        4000U
#endif

/* 编码器更新周期（us），必须与调用 encoder_update() 的任务周期一致 */
#ifndef ENCODER_UPDATE_PERIOD_US
#define ENCODER_UPDATE_PERIOD_US      1000U
#endif

/* 单圈机械零位偏移（counts），从单圈计数中减去；0 表示不偏移 */
#ifndef ENCODER_DEFAULT_OFFSET_COUNTS
#define ENCODER_DEFAULT_OFFSET_COUNTS         0
#endif

/* 翻转计数方向 */
#ifndef ENCODER_DEFAULT_INVERT
#define ENCODER_DEFAULT_INVERT                false
#endif

/* 速度低通截止频率（Hz），0 表示关闭 */
#ifndef ENCODER_DEFAULT_VELOCITY_LPF_HZ
#define ENCODER_DEFAULT_VELOCITY_LPF_HZ       0.0f
#endif

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
