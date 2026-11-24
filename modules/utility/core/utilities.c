/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stddef.h>
#include "utilities.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/
#define UTILITY_TWO_PI (6.28318530717958647692f)

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief 浮点数范围限制
 * @param value 输入的浮点数值
 * @param min 最小值
 * @param max 最大值
 * @return float 限制后的浮点数值
 */
float clamp_float(float value, float min, float max)
{
    if (value > max)
    {
        return max;
    }

    if (value < min)
    {
        return min;
    }

    return value;
}

/**
 * @brief 浮点数转换为16位整数
 * @param x 输入的浮点数值
 * @param x_min 浮点数的最小值
 * @param x_max 浮点数的最大值
 * @return int16_t 转换后的16位有符号整数
 */
int16_t float_to_int16(float x, float x_min, float x_max)
{
    float span = x_max - x_min;
    float scale;

    if (x > x_max)
    {
        x = x_max;
    }
    else if (x < x_min)
    {
        x = x_min;
    }

    scale = 65534.0f / span;

    return (int16_t)((x - (x_min + x_max) * 0.5f) * scale);
}

/**
 * @brief 浮点数转换为8位整数
 * @param x 输入的浮点数值
 * @param x_min 浮点数的最小值
 * @param x_max 浮点数的最大值
 * @return int8_t 转换后的8位有符号整数
 */
int8_t float_to_int8(float x, float x_min, float x_max)
{
    float span = x_max - x_min;
    float scale;

    if (x > x_max)
    {
        x = x_max;
    }
    else if (x < x_min)
    {
        x = x_min;
    }

    scale = 254.0f / span;

    return (int8_t)((x - (x_min + x_max) * 0.5f) * scale);
}

/**
 * @brief 计算一阶低通滤波系数
 *
 * @param dt_s 采样周期（s）
 * @param cutoff_hz 截止频率（Hz），<= 0 返回 1（直通）
 * @return 滤波系数，dt_s <= 0 时返回 0（保持上次状态）
 */
float first_order_lpf_alpha(float dt_s, float cutoff_hz)
{
    float tau;

    /* 关闭滤波时系数为 1，状态直接跟随输入 */
    if (cutoff_hz <= 0.0f)
    {
        return 1.0f;
    }

    /* 采样周期无效时系数为 0，状态保持不变 */
    if (dt_s <= 0.0f)
    {
        return 0.0f;
    }

    tau = 1.0f / (UTILITY_TWO_PI * cutoff_hz);
    return dt_s / (tau + dt_s);
}

/**
 * @brief 一阶低通滤波
 *
 * @param state 滤波器状态指针（输入/输出），为 NULL 时直接返回 input
 * @param input 新采样值
 * @param alpha 滤波系数，范围 [0, 1]
 * @return 滤波后的值
 */
float first_order_lpf(float *state, float input, float alpha)
{
    if (state == NULL)
    {
        return input;
    }

    *state += alpha * (input - *state);
    return *state;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
