#ifndef WS2816_H
#define WS2816_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WS2816_LED_COUNT             232U
#define WS2816_CHANNELS_PER_LED        3U
#define WS2816_CURRENT_GAIN_MAX       31U

typedef uint16_t ws2816_value_t;

typedef struct
{
    uint8_t green;
    uint8_t red;
    uint8_t blue;
} ws2816_gain_t;

typedef enum
{
    WS2816_COLOR_OFF = 0,
    WS2816_COLOR_BLACK = 0,
    WS2816_COLOR_RED,
    WS2816_COLOR_GREEN,
    WS2816_COLOR_BLUE,
    WS2816_COLOR_WHITE,
    WS2816_COLOR_YELLOW,
    WS2816_COLOR_CYAN,
    WS2816_COLOR_MAGENTA,
    WS2816_COLOR_ORANGE,
    WS2816_COLOR_PURPLE,
    WS2816_COLOR_PINK,
    WS2816_COLOR_SKY_BLUE,
    WS2816_COLOR_GOLD,
    WS2816_COLOR_SILVER,
    WS2816_COLOR_WARM_WHITE,
    WS2816_COLOR_COOL_WHITE,
    WS2816_COLOR_COUNT
} ws2816_color_t;

typedef struct
{
    int (*init)(void);
    int (*transmit)(const ws2816_value_t *pixels, uint32_t led_count,
                    uint8_t brightness, const ws2816_gain_t *gain);
    int (*deinit)(void);
} ws2816_driver_t;

int ws2816_init(uint32_t led_count);
int ws2816_deinit(void);

void ws2816_set_pixel(uint32_t index, ws2816_value_t red,
                      ws2816_value_t green, ws2816_value_t blue);
void ws2816_set_all(ws2816_value_t red, ws2816_value_t green,
                    ws2816_value_t blue);
void ws2816_set_pixel_color(uint32_t index, ws2816_color_t color);
void ws2816_set_all_color(ws2816_color_t color);
void ws2816_set_brightness(uint8_t brightness);
void ws2816_set_current_gain(uint8_t green, uint8_t red, uint8_t blue);
void ws2816_clear(void);
void ws2816_show(void);
uint32_t ws2816_get_led_count(void);

void ws2816_hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value,
                       ws2816_value_t *red, ws2816_value_t *green,
                       ws2816_value_t *blue);
void ws2816_color_to_rgb(ws2816_color_t color, ws2816_value_t *red,
                         ws2816_value_t *green, ws2816_value_t *blue);

static inline ws2816_value_t ws2816_expand_u8(uint8_t value)
{
    return (ws2816_value_t)((uint16_t)value * 257U);
}

#ifdef __cplusplus
}
#endif

#endif

