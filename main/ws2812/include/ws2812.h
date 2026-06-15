#ifndef __WS2812_H_
#define __WS2812_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws2812_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/**
 * @brief  WS2812 预定义颜色枚举
 * @note   通过语义化名称直观设置颜色，避免记忆 RGB 数值
 */
typedef enum {
    WS2812_COLOR_OFF    = 0,
    WS2812_COLOR_BLACK  = 0,
    WS2812_COLOR_RED,
    WS2812_COLOR_GREEN,
    WS2812_COLOR_BLUE,
    WS2812_COLOR_WHITE,
    WS2812_COLOR_YELLOW,
    WS2812_COLOR_CYAN,
    WS2812_COLOR_MAGENTA,
    WS2812_COLOR_ORANGE,
    WS2812_COLOR_PURPLE,
    WS2812_COLOR_PINK,
    WS2812_COLOR_SKYBLUE,
    WS2812_COLOR_GOLD,
    WS2812_COLOR_SILVER,
    WS2812_COLOR_WARM_WHITE,
    WS2812_COLOR_COOL_WHITE,
    WS2812_COLOR_NUM
} ws2812_color_t;

#if WS281X_DEVICE == WS281X_DEV_WS2816A
typedef uint16_t ws281x_pixel_t;
#else
typedef uint8_t  ws281x_pixel_t;
#endif

#if WS281X_HAS_GAIN
typedef struct {
    uint8_t gain_g;
    uint8_t gain_r;
    uint8_t gain_b;
} ws281x_gain_t;
#endif

/**
 * @brief  WS2812 硬件驱动抽象接口
 * @note   上层 core 层不关心具体硬件实现，只通过此接口调用 port 层。
 *         每个平台只需实现这四个函数指针即可完成移植。
 */
typedef struct ws2812_driver {
    int (*init)(void);
    int (*transmit)(const uint8_t *data, uint32_t len);
    int (*is_busy)(void);
    int (*deinit)(void);
} ws2812_driver_t;

/****************************************************************************/
/*						Exproted Variables									*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions									*/
/****************************************************************************/

/**
 * @brief  初始化 WS2812 驱动库
 * @param  num_leds LED 灯珠数量
 * @return 0: 成功, -1: 参数错误, -3: 硬件初始化失败
 */
int  ws2812_init(uint32_t num_leds);

/**
 * @brief  反初始化 WS2812 驱动库
 * @return 0: 成功
 */
int  ws2812_deinit(void);

/**
 * @brief  设置单颗灯珠 RGB 颜色
 * @param  index 灯珠索引 (0 ~ num_leds-1)
 * @param  r     红色分量
 * @param  g     绿色分量
 * @param  b     蓝色分量
 */
void ws2812_set_pixel(uint32_t index, ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b);

/**
 * @brief  设置所有灯珠为同一 RGB 颜色
 * @param  r 红色分量
 * @param  g 绿色分量
 * @param  b 蓝色分量
 */
void ws2812_set_all(ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b);

/**
 * @brief  使用预定义颜色枚举设置单颗灯珠
 * @param  index 灯珠索引
 * @param  color 预定义颜色枚举值
 */
void ws2812_set_pixel_color(uint32_t index, ws2812_color_t color);

/**
 * @brief  使用预定义颜色枚举设置所有灯珠
 * @param  color 预定义颜色枚举值
 */
void ws2812_set_all_color(ws2812_color_t color);

/**
 * @brief  设置全局亮度
 * @param  brightness 亮度值 (0~255), 255 为全亮, 0 为全灭
 */
void     ws2812_set_brightness(uint8_t brightness);

/**
 * @brief  获取 LED 灯珠数量
 * @return LED 灯珠数量
 */
uint32_t ws2812_get_num_leds(void);

/**
 * @brief  熄灭所有灯珠
 */
void     ws2812_clear(void);

/**
 * @brief  将缓冲区数据推送到灯带（阻塞）
 * @note   若上一次传输未完成，会忙等待直到完成
 */
void ws2812_show(void);

/**
 * @brief  将缓冲区数据推送到灯带（非阻塞）
 * @return true: 发送已启动, false: 上次发送未完成
 */
bool ws2812_show_async(void);

/**
 * @brief  查询是否有传输正在进行
 * @return true: 正在发送, false: 空闲
 */
bool ws2812_is_busy(void);

#if WS281X_HAS_GAIN
/**
 * @brief  设置全局增益（仅 WS2816A 等支持增益的设备有效）
 * @param  gain_g 绿色增益 (5bit, 0~31)
 * @param  gain_r 红色增益 (5bit, 0~31)
 * @param  gain_b 蓝色增益 (5bit, 0~31)
 */
void ws2812_set_gain(uint8_t gain_g, uint8_t gain_r, uint8_t gain_b);
#endif

/****************************************************************************/
/*                        Effect Functions                                  */
/****************************************************************************/

/**
 * @brief  呼吸灯效果
 * @param  r          红色分量 (0~255)
 * @param  g          绿色分量 (0~255)
 * @param  b          蓝色分量 (0~255)
 * @param  period_ms  呼吸周期 (ms)
 * @note   需要周期性调用（如每 10ms），调用后需显式调用 ws2812_show()
 */
void ws2812_effect_breath(uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms);

/**
 * @brief  彩虹循环效果
 * @param  speed       速度 (0~255)，越大越快
 * @param  brightness  亮度 (0~255)
 * @note   需要周期性调用，调用后需显式调用 ws2812_show()
 */
void ws2812_effect_rainbow(uint8_t speed, uint8_t brightness);

/**
 * @brief  跑马灯效果
 * @param  r         红色分量 (0~255)
 * @param  g         绿色分量 (0~255)
 * @param  b         蓝色分量 (0~255)
 * @param  speed_ms  移动间隔 (ms)
 * @param  size      亮灯组大小
 * @note   需要周期性调用，调用后需显式调用 ws2812_show()
 */
void ws2812_effect_chase(uint8_t r, uint8_t g, uint8_t b, uint32_t speed_ms, uint8_t size);

/****************************************************************************/
/*                        Color Utility Functions                           */
/****************************************************************************/

/**
 * @brief  HSV 色彩空间转 RGB
 * @param  h  色调 (0~359)
 * @param  s  饱和度 (0~255)
 * @param  v  明度 (0~255)
 * @param  r  输出红色分量
 * @param  g  输出绿色分量
 * @param  b  输出蓝色分量
 */
void ws2812_hsv2rgb(uint16_t h, uint8_t s, uint8_t v,
                     ws281x_pixel_t *r, ws281x_pixel_t *g, ws281x_pixel_t *b);

/**
 * @brief  根据预定义颜色枚举获取 RGB 值
 * @param  color 预定义颜色枚举
 * @param  r     输出红色分量
 * @param  g     输出绿色分量
 * @param  b     输出蓝色分量
 */
void ws2812_color_get_rgb(ws2812_color_t color,
                           ws281x_pixel_t *r, ws281x_pixel_t *g, ws281x_pixel_t *b);

/**
 * @brief  将亮度缩放应用到 RGB 值
 * @param  r          红色分量输入/输出
 * @param  g          绿色分量输入/输出
 * @param  b          蓝色分量输入/输出
 * @param  brightness 亮度 (0~255)
 */
void ws2812_apply_brightness(ws281x_pixel_t *r, ws281x_pixel_t *g, ws281x_pixel_t *b, uint8_t brightness);

/**
 * @brief  将 8bit 颜色值扩展为当前设备像素位宽
 * @param  val 8bit 输入值
 * @return WS2812: 原值; WS2816A: val * 257 (16bit 扩展)
 */
static inline ws281x_pixel_t ws281x_expand_8bit(uint8_t val)
{
#if WS281X_DEVICE == WS281X_DEV_WS2816A
    return (ws281x_pixel_t)((uint16_t)val * 257U);
#else
    return (ws281x_pixel_t)val;
#endif
}

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/