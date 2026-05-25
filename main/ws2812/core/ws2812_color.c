/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws2812.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/**
 * @brief  内部 RGB 颜色结构体
 */
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_rgb_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/**
 * @brief  预定义颜色查找表
 * @note   与 ws2812_color_t 枚举一一对应
 */
static const ws2812_rgb_t g_color_table[WS2812_COLOR_NUM] = {
    [WS2812_COLOR_OFF]        = { 0x00, 0x00, 0x00 },
    [WS2812_COLOR_RED]        = { 0xFF, 0x00, 0x00 },
    [WS2812_COLOR_GREEN]      = { 0x00, 0xFF, 0x00 },
    [WS2812_COLOR_BLUE]       = { 0x00, 0x00, 0xFF },
    [WS2812_COLOR_WHITE]      = { 0xFF, 0xFF, 0xFF },
    [WS2812_COLOR_YELLOW]     = { 0xFF, 0xFF, 0x00 },
    [WS2812_COLOR_CYAN]       = { 0x00, 0xFF, 0xFF },
    [WS2812_COLOR_MAGENTA]    = { 0xFF, 0x00, 0xFF },
    [WS2812_COLOR_ORANGE]     = { 0xFF, 0x80, 0x00 },
    [WS2812_COLOR_PURPLE]     = { 0xFF, 0x00, 0xFF },
    [WS2812_COLOR_PINK]       = { 0xFF, 0xC0, 0xCB },
    [WS2812_COLOR_SKYBLUE]    = { 0x87, 0xCE, 0xEB },
    [WS2812_COLOR_GOLD]       = { 0xFF, 0xD7, 0x00 },
    [WS2812_COLOR_SILVER]     = { 0xC0, 0xC0, 0xC0 },
    [WS2812_COLOR_WARM_WHITE] = { 0xFF, 0xF0, 0xE0 },
    [WS2812_COLOR_COOL_WHITE] = { 0xF0, 0xF0, 0xFF },
};

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  根据预定义颜色枚举获取对应的 RGB 值
 * @param  color 预定义颜色枚举
 * @param  r     输出红色分量
 * @param  g     输出绿色分量
 * @param  b     输出蓝色分量
 */
void ws2812_color_get_rgb(ws2812_color_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (color >= WS2812_COLOR_NUM) 
    {
        color = WS2812_COLOR_OFF;
    }

    *r = g_color_table[color].r;
    *g = g_color_table[color].g;
    *b = g_color_table[color].b;
}

/**
 * @brief  将亮度缩放应用到 RGB 各分量
 * @param  r          红色分量输入/输出
 * @param  g          绿色分量输入/输出
 * @param  b          蓝色分量输入/输出
 * @param  brightness 亮度值 (0~255)
 */
void ws2812_apply_brightness(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t brightness)
{
    if (brightness == 255) 
    {
        return;
    }
    *r = (uint8_t)(((uint16_t)*r * brightness) / 255U);
    *g = (uint8_t)(((uint16_t)*g * brightness) / 255U);
    *b = (uint8_t)(((uint16_t)*b * brightness) / 255U);
}

/**
 * @brief  HSV 色彩空间转换为 RGB
 * @param  h  色调 (0~359)
 * @param  s  饱和度 (0~255)
 * @param  v  明度 (0~255)
 * @param  r  输出红色分量
 * @param  g  输出绿色分量
 * @param  b  输出蓝色分量
 */
void ws2812_hsv2rgb(uint16_t h, uint8_t s, uint8_t v,
                     uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region;
    uint8_t p, q, t;
    uint16_t remainder;

    if (s == 0) 
    {
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    h %= 360;
    region = (uint8_t)(h / 60);
    remainder = (uint16_t)(((uint32_t)(h % 60) * 255U) / 60U);

    p = (uint8_t)(((uint16_t)v * (255U - s)) / 255U);
    q = (uint8_t)(((uint16_t)v * (255U - ((uint16_t)s * remainder) / 255U)) / 255U);
    t = (uint8_t)(((uint16_t)v * (255U - ((uint16_t)s * (255U - remainder)) / 255U)) / 255U);

    switch (region) 
    {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
