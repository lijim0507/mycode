#ifndef __UTILITIES_H_
#define __UTILITIES_H_
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

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

float   clamp_float(float value, float min, float max);
int16_t float_to_int16(float x, float x_min, float x_max);
int8_t  float_to_int8(float x, float x_min, float x_max);

/**
 * @brief 计算一阶低通滤波系数
 *
 * @param dt_s 采样周期（s）
 * @param cutoff_hz 截止频率（Hz），<= 0 返回 1（直通）
 * @return 滤波系数
 */
float   first_order_lpf_alpha(float dt_s, float cutoff_hz);

/**
 * @brief 一阶低通滤波
 *
 * @param state 滤波器状态指针（输入/输出），为 NULL 时直接返回 input
 * @param input 新采样值
 * @param alpha 滤波系数
 * @return 滤波后的值
 */
float   first_order_lpf(float *state, float input, float alpha);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
