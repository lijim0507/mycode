#include "ws2816.h"

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb8_t;

static const rgb8_t s_colors[WS2816_COLOR_COUNT] = {
    [WS2816_COLOR_OFF]        = {0x00, 0x00, 0x00},
    [WS2816_COLOR_RED]        = {0xFF, 0x00, 0x00},
    [WS2816_COLOR_GREEN]      = {0x00, 0xFF, 0x00},
    [WS2816_COLOR_BLUE]       = {0x00, 0x00, 0xFF},
    [WS2816_COLOR_WHITE]      = {0xFF, 0xFF, 0xFF},
    [WS2816_COLOR_YELLOW]     = {0xFF, 0xFF, 0x00},
    [WS2816_COLOR_CYAN]       = {0x00, 0xFF, 0xFF},
    [WS2816_COLOR_MAGENTA]    = {0xFF, 0x00, 0xFF},
    [WS2816_COLOR_ORANGE]     = {0xFF, 0x80, 0x00},
    [WS2816_COLOR_PURPLE]     = {0xFF, 0x00, 0xFF},
    [WS2816_COLOR_PINK]       = {0xFF, 0xC0, 0xCB},
    [WS2816_COLOR_SKY_BLUE]   = {0x87, 0xCE, 0xEB},
    [WS2816_COLOR_GOLD]       = {0xFF, 0xD7, 0x00},
    [WS2816_COLOR_SILVER]     = {0xC0, 0xC0, 0xC0},
    [WS2816_COLOR_WARM_WHITE] = {0xFF, 0xF0, 0xE0},
    [WS2816_COLOR_COOL_WHITE] = {0xF0, 0xF0, 0xFF},
};

void ws2816_color_to_rgb(ws2816_color_t color, ws2816_value_t *red,
                         ws2816_value_t *green, ws2816_value_t *blue)
{
    if (color >= WS2816_COLOR_COUNT)
    {
        color = WS2816_COLOR_OFF;
    }

    *red = ws2816_expand_u8(s_colors[color].red);
    *green = ws2816_expand_u8(s_colors[color].green);
    *blue = ws2816_expand_u8(s_colors[color].blue);
}

void ws2816_hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value,
                       ws2816_value_t *red, ws2816_value_t *green,
                       ws2816_value_t *blue)
{
    uint8_t region;
    uint8_t minimum;
    uint8_t falling;
    uint8_t rising;
    uint16_t fraction;
    uint8_t red8;
    uint8_t green8;
    uint8_t blue8;

    if (saturation == 0U)
    {
        *red = ws2816_expand_u8(value);
        *green = ws2816_expand_u8(value);
        *blue = ws2816_expand_u8(value);
        return;
    }

    hue %= 360U;
    region = (uint8_t)(hue / 60U);
    fraction = (uint16_t)(((uint32_t)(hue % 60U) * 255U) / 60U);
    minimum = (uint8_t)(((uint16_t)value * (255U - saturation)) / 255U);
    falling = (uint8_t)(((uint16_t)value
              * (255U - ((uint16_t)saturation * fraction) / 255U)) / 255U);
    rising = (uint8_t)(((uint16_t)value
             * (255U - ((uint16_t)saturation * (255U - fraction)) / 255U)) / 255U);

    switch (region)
    {
        case 0: red8 = value; green8 = rising; blue8 = minimum; break;
        case 1: red8 = falling; green8 = value; blue8 = minimum; break;
        case 2: red8 = minimum; green8 = value; blue8 = rising; break;
        case 3: red8 = minimum; green8 = falling; blue8 = value; break;
        case 4: red8 = rising; green8 = minimum; blue8 = value; break;
        default: red8 = value; green8 = minimum; blue8 = falling; break;
    }

    *red = ws2816_expand_u8(red8);
    *green = ws2816_expand_u8(green8);
    *blue = ws2816_expand_u8(blue8);
}

