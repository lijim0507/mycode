#ifndef __WS281X_H_
#define __WS281X_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws281x_config.h"
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
 * @brief  WS281X 设备类型枚举
 * @note   在 ws281x_init() 时选择，决定协议编码方式
 */
typedef enum {
    WS281X_DEV_WS2812  = 1,
    WS281X_DEV_WS2816A = 2,
} ws281x_dev_type_t;

/**
 * @brief  WS281X 预定义颜色枚举
 * @note   通过语义化名称直观设置颜色，避免记忆 RGB 数值
 */
typedef enum {
    WS281X_COLOR_OFF    = 0,
    WS281X_COLOR_BLACK  = 0,
    WS281X_COLOR_RED,
    WS281X_COLOR_GREEN,
    WS281X_COLOR_BLUE,
    WS281X_COLOR_WHITE,
    WS281X_COLOR_YELLOW,
    WS281X_COLOR_CYAN,
    WS281X_COLOR_MAGENTA,
    WS281X_COLOR_ORANGE,
    WS281X_COLOR_PURPLE,
    WS281X_COLOR_PINK,
    WS281X_COLOR_SKYBLUE,
    WS281X_COLOR_GOLD,
    WS281X_COLOR_SILVER,
    WS281X_COLOR_WARM_WHITE,
    WS281X_COLOR_COOL_WHITE,
    WS281X_COLOR_NUM,
} ws281x_color_t;

typedef uint16_t ws281x_pixel_t;

/**
 * @brief  WS281X 设备句柄（不透明指针）
 * @note   通过 ws281x_init() 获取，后续所有操作通过句柄进行
 */
typedef struct ws281x_dev *ws281x_handle_t;

/****************************************************************************/
/*						Exproted Variables									*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions									*/
/****************************************************************************/

/**
 * @brief  初始化 WS281X 驱动库，返回设备句柄
 * @param  num_leds LED 灯珠数量
 * @param  dev_type 设备类型 (WS281X_DEV_WS2812 或 WS281X_DEV_WS2816A)
 * @return 非NULL: 成功返回设备句柄, NULL: 失败
 */
ws281x_handle_t ws281x_init(uint32_t num_leds, ws281x_dev_type_t dev_type);

/**
 * @brief  反初始化 WS281X 驱动库
 * @param  dev 设备句柄
 * @return 0: 成功, -1: 参数错误
 */
int  ws281x_deinit(ws281x_handle_t dev);

/**
 * @brief  设置单颗灯珠 RGB 颜色
 * @param  dev   设备句柄
 * @param  index 灯珠索引 (0 ~ num_leds-1)
 * @param  r     红色分量 (WS2812: 0~255; WS2816A: 0~65535)
 * @param  g     绿色分量
 * @param  b     蓝色分量
 */
void ws281x_set_pixel(ws281x_handle_t dev, uint32_t index, ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b);

/**
 * @brief  设置所有灯珠为同一 RGB 颜色
 * @param  dev 设备句柄
 * @param  r   红色分量
 * @param  g   绿色分量
 * @param  b   蓝色分量
 */
void ws281x_set_all(ws281x_handle_t dev, ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b);

/**
 * @brief  使用预定义颜色枚举设置单颗灯珠
 * @param  dev   设备句柄
 * @param  index 灯珠索引
 * @param  color 预定义颜色枚举值
 */
void ws281x_set_pixel_color(ws281x_handle_t dev, uint32_t index, ws281x_color_t color);

/**
 * @brief  使用预定义颜色枚举设置所有灯珠
 * @param  dev   设备句柄
 * @param  color 预定义颜色枚举值
 */
void ws281x_set_all_color(ws281x_handle_t dev, ws281x_color_t color);

/**
 * @brief  设置全局亮度
 * @param  dev         设备句柄
 * @param  brightness 亮度值 (0~255), 255 为全亮, 0 为全灭
 */
void ws281x_set_brightness(ws281x_handle_t dev, uint8_t brightness);

/**
 * @brief  设置全局增益（仅 WS2816A 等支持增益的设备有效）
 * @param  dev    设备句柄
 * @param  gain_g 绿色增益 (5bit, 0~31)
 * @param  gain_r 红色增益 (5bit, 0~31)
 * @param  gain_b 蓝色增益 (5bit, 0~31)
 * @note   WS2812 设备调用此函数无效
 */
void ws281x_set_gain(ws281x_handle_t dev, uint8_t gain_g, uint8_t gain_r, uint8_t gain_b);

/**
 * @brief  获取 LED 灯珠数量
 * @param  dev 设备句柄
 * @return LED 灯珠数量
 */
uint32_t ws281x_get_num_leds(ws281x_handle_t dev);

/**
 * @brief  熄灭所有灯珠
 * @param  dev 设备句柄
 */
void ws281x_clear(ws281x_handle_t dev);

/**
 * @brief  将缓冲区数据推送到灯带（阻塞）
 * @param  dev 设备句柄
 * @note   若上一次传输未完成，会忙等待直到完成
 */
void ws281x_show(ws281x_handle_t dev);

/**
 * @brief  将缓冲区数据推送到灯带（非阻塞）
 * @param  dev 设备句柄
 * @return true: 发送已启动, false: 上次发送未完成或参数错误
 */
bool ws281x_show_async(ws281x_handle_t dev);

/**
 * @brief  查询是否有传输正在进行
 * @param  dev 设备句柄
 * @return true: 正在发送, false: 空闲或参数错误
 */
bool ws281x_is_busy(ws281x_handle_t dev);

/****************************************************************************/
/*                        Effect Functions                                  */
/****************************************************************************/

/**
 * @brief  呼吸灯效果
 * @param  dev        设备句柄
 * @param  r          红色分量 (0~255)
 * @param  g          绿色分量 (0~255)
 * @param  b          蓝色分量 (0~255)
 * @param  period_ms  呼吸周期 (ms)
 * @note   需要周期性调用（如每 10ms），调用后需显式调用 ws281x_show()
 */
void ws281x_effect_breath(ws281x_handle_t dev, uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms);

/**
 * @brief  彩虹循环效果
 * @param  dev         设备句柄
 * @param  speed       速度 (0~255)，越大越快
 * @param  brightness  亮度 (0~255)
 * @note   需要周期性调用，调用后需显式调用 ws281x_show()
 */
void ws281x_effect_rainbow(ws281x_handle_t dev, uint8_t speed, uint8_t brightness);

/**
 * @brief  跑马灯效果
 * @param  dev       设备句柄
 * @param  r         红色分量 (0~255)
 * @param  g         绿色分量 (0~255)
 * @param  b         蓝色分量 (0~255)
 * @param  speed_ms  移动间隔 (ms)
 * @param  size      亮灯组大小
 * @note   需要周期性调用，调用后需显式调用 ws281x_show()
 */
void ws281x_effect_chase(ws281x_handle_t dev, uint8_t r, uint8_t g, uint8_t b, uint32_t speed_ms, uint8_t size);

/****************************************************************************/
/*                        Color Utility Functions                           */
/****************************************************************************/

/**
 * @brief  HSV 色彩空间转 RGB (8-bit 范围)
 * @param  h  色调 (0~359)
 * @param  s  饱和度 (0~255)
 * @param  v  明度 (0~255)
 * @param  r  输出红色分量 (0~255)
 * @param  g  输出绿色分量 (0~255)
 * @param  b  输出蓝色分量 (0~255)
 */
void ws281x_hsv2rgb(uint16_t h, uint8_t s, uint8_t v,
                     uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * @brief  根据预定义颜色枚举获取 RGB 值 (8-bit 范围)
 * @param  color 预定义颜色枚举
 * @param  r     输出红色分量 (0~255)
 * @param  g     输出绿色分量 (0~255)
 * @param  b     输出蓝色分量 (0~255)
 */
void ws281x_color_get_rgb(ws281x_color_t color, uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * @brief  将 8bit 颜色值扩展为 16bit (65535 范围)
 * @param  val 8bit 输入值 (0~255)
 * @return val * 257，映射 0~255 到 0~65535
 */
static inline ws281x_pixel_t ws281x_expand_8bit(uint8_t val)
{
    return (ws281x_pixel_t)((uint16_t)val * 257U);
}

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/