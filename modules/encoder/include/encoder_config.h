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
#ifndef ENCODER_DEFAULT_COUNTS_PER_REV
#define ENCODER_DEFAULT_COUNTS_PER_REV      4000U
#endif

/* 翻转计数方向 */
#ifndef ENCODER_DEFAULT_INVERT
#define ENCODER_DEFAULT_INVERT              false
#endif

/* 速度低通截止频率（Hz），0 表示关闭 */
#ifndef ENCODER_DEFAULT_VELOCITY_LPF_HZ
#define ENCODER_DEFAULT_VELOCITY_LPF_HZ     0.0f
#endif

/* 速度死区（counts/s），0 表示关闭 */
#ifndef ENCODER_DEFAULT_VELOCITY_DEADBAND
#define ENCODER_DEFAULT_VELOCITY_DEADBAND   0.0f
#endif

/* 首次 Z 脉冲时清零，port 支持 Z 时生效 */
#ifndef ENCODER_DEFAULT_ZERO_ON_INDEX
#define ENCODER_DEFAULT_ZERO_ON_INDEX       false
#endif

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
