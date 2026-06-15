/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws281x.h"

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
} ws281x_rgb_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/**
 * @brief  预定义颜色查找表
 * @note   与 ws281x_color_t 枚举一一对应
 */
static const ws281x_rgb_t g_color_table[WS281X_COLOR_NUM] = {
    [WS281X_COLOR_OFF]        = { 0x00, 0x00, 0x00 },
    [WS281X_COLOR_RED]        = { 0xFF, 0x00, 0x00 },
    [WS281X_COLOR_GREEN]      = { 0x00, 0xFF, 0x00 },
    [WS281X_COLOR_BLUE]       = { 0x00, 0x00, 0xFF },
    [WS281X_COLOR_WHITE]      = { 0xFF, 0xFF, 0xFF },
    [WS281X_COLOR_YELLOW]     = { 0xFF, 0xFF, 0x00 },
    [WS281X_COLOR_CYAN]       = { 0x00, 0xFF, 0xFF },
    [WS281X_COLOR_MAGENTA]    = { 0xFF, 0x00, 0xFF },
    [WS281X_COLOR_ORANGE]     = { 0xFF, 0x80, 0x00 },
    [WS281X_COLOR_PURPLE]     = { 0xFF, 0x00, 0xFF },
    [WS281X_COLOR_PINK]       = { 0xFF, 0xC0, 0xCB },
    [WS281X_COLOR_SKYBLUE]    = { 0x87, 0xCE, 0xEB },
    [WS281X_COLOR_GOLD]       = { 0xFF, 0xD7, 0x00 },
    [WS281X_COLOR_SILVER]     = { 0xC0, 0xC0, 0xC0 },
    [WS281X_COLOR_WARM_WHITE] = { 0xFF, 0xF0, 0xE0 },
    [WS281X_COLOR_COOL_WHITE] = { 0xF0, 0xF0, 0xFF },
};

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  根据预定义颜色枚举获取对应的 RGB 值 (8-bit 范围)
 * @param  color 预定义颜色枚举
 * @param  r     输出红色分量 (0~255)
 * @param  g     输出绿色分量 (0~255)
 * @param  b     输出蓝色分量 (0~255)
 */
void ws281x_color_get_rgb(ws281x_color_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (color >= WS281X_COLOR_NUM)
    {
        color = WS281X_COLOR_OFF;
    }

    *r = g_color_table[color].r;
    *g = g_color_table[color].g;
    *b = g_color_table[color].b;
}

/**
 * @brief  HSV 色彩空间转换为 RGB (8-bit 范围)
 * @param  h  色调 (0~359)
 * @param  s  饱和度 (0~255)
 * @param  v  明度 (0~255)
 * @param  r  输出红色分量 (0~255)
 * @param  g  输出绿色分量 (0~255)
 * @param  b  输出蓝色分量 (0~255)
 */
void ws281x_hsv2rgb(uint16_t h, uint8_t s, uint8_t v,
                     uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region, p, q, t;
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