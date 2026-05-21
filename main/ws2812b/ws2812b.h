
#ifndef __WS2812B_H_
#define __WS2812B_H_
/****************************************************************************/
/*							Includes									*/
/****************************************************************************/
#include "stdint.h"

/****************************************************************************/
/*							Macros										*/
/****************************************************************************/
#define WS2812B_PIXELS_NUM      4

/****************************************************************************/
/*							Typedefs										*/
/****************************************************************************/
/**
 * @brief  WS2812B 像素颜色结构体 (GRB 顺序，与 WS2812B 硬件时序一致)
 */
typedef struct _ws2812b_t
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
}ws2812b_t;

/**
 * @brief  LED 预定义颜色枚举
 * @note   通过语义化名称直观设置颜色，避免记忆 RGB 数值
 */
typedef enum _led_color_t
{
    LED_COLOR_OFF = 0,      /* 熄灭 */
    LED_COLOR_BLACK,        /* 黑色 (同 OFF) */
    LED_COLOR_RED,          /* 红色 */
    LED_COLOR_GREEN,        /* 绿色 */
    LED_COLOR_BLUE,         /* 蓝色 */
    LED_COLOR_WHITE,        /* 白色 */
    LED_COLOR_YELLOW,       /* 黄色 */
    LED_COLOR_CYAN,         /* 青色 */
    LED_COLOR_MAGENTA,      /* 品红 */
    LED_COLOR_ORANGE,       /* 橙色 */
    LED_COLOR_PURPLE,       /* 紫色 */
    LED_COLOR_PINK,         /* 粉红 */
    LED_COLOR_LIME,         /* 酸橙绿 */
    LED_COLOR_SKYBLUE,      /* 天蓝 */
    LED_COLOR_GOLD,         /* 金色 */
    LED_COLOR_SILVER,       /* 银色 */
    LED_COLOR_NUM           /* 颜色总数，用于边界检查 */
} led_color_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/
extern ws2812b_t ws2812b_pixels[WS2812B_PIXELS_NUM];

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/
void ws2812b_init(void);
void ws2812b_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void ws2812b_set_all(uint8_t r, uint8_t g, uint8_t b);
void ws2812b_update(void);
void ws2812b_set_pixel_color(uint8_t index, led_color_t color);
void ws2812b_set_all_color(led_color_t color);

#endif
/****************************************************************************/
/*							EOF											*/
/****************************************************************************/
