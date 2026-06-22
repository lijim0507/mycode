#include "ws2816.h"
#include "drv_ws2816_spi.h"

#include <string.h>

static const ws2816_driver_t *s_driver;
static ws2816_value_t s_pixels[WS2816_LED_COUNT * WS2816_CHANNELS_PER_LED];
static uint32_t s_led_count;
static uint8_t s_brightness = 255U;
static bool s_initialized;
static ws2816_gain_t s_gain = {
    WS2816_CURRENT_GAIN_MAX,
    WS2816_CURRENT_GAIN_MAX,
    WS2816_CURRENT_GAIN_MAX
};

int ws2816_init(uint32_t led_count)
{
    if (led_count == 0U || led_count > WS2816_LED_COUNT)
    {
        return -1;
    }

    if (s_initialized)
    {
        ws2816_deinit();
    }

    s_driver = ws2816_spi_driver_get();
    if (s_driver == NULL || s_driver->init == NULL || s_driver->transmit == NULL)
    {
        return -2;
    }

    s_led_count = led_count;
    s_brightness = 255U;
    memset(s_pixels, 0, sizeof(s_pixels));

    if (s_driver->init() != 0)
    {
        s_driver = NULL;
        s_led_count = 0U;
        return -3;
    }

    s_initialized = true;
    return 0;
}

int ws2816_deinit(void)
{
    if (!s_initialized)
    {
        return 0;
    }

    if (s_driver != NULL && s_driver->deinit != NULL)
    {
        s_driver->deinit();
    }

    s_driver = NULL;
    s_led_count = 0U;
    s_initialized = false;
    return 0;
}

void ws2816_set_pixel(uint32_t index, ws2816_value_t red,
                      ws2816_value_t green, ws2816_value_t blue)
{
    uint32_t offset;

    if (!s_initialized || index >= s_led_count)
    {
        return;
    }

    offset = index * WS2816_CHANNELS_PER_LED;
    s_pixels[offset] = green;
    s_pixels[offset + 1U] = red;
    s_pixels[offset + 2U] = blue;
}

void ws2816_set_all(ws2816_value_t red, ws2816_value_t green,
                    ws2816_value_t blue)
{
    if (!s_initialized)
    {
        return;
    }

    for (uint32_t i = 0; i < s_led_count; i++)
    {
        ws2816_set_pixel(i, red, green, blue);
    }
}

void ws2816_set_pixel_color(uint32_t index, ws2816_color_t color)
{
    ws2816_value_t red;
    ws2816_value_t green;
    ws2816_value_t blue;

    ws2816_color_to_rgb(color, &red, &green, &blue);
    ws2816_set_pixel(index, red, green, blue);
}

void ws2816_set_all_color(ws2816_color_t color)
{
    ws2816_value_t red;
    ws2816_value_t green;
    ws2816_value_t blue;

    ws2816_color_to_rgb(color, &red, &green, &blue);
    ws2816_set_all(red, green, blue);
}

void ws2816_set_brightness(uint8_t brightness)
{
    s_brightness = brightness;
}

void ws2816_set_current_gain(uint8_t green, uint8_t red, uint8_t blue)
{
    s_gain.green = green & WS2816_CURRENT_GAIN_MAX;
    s_gain.red = red & WS2816_CURRENT_GAIN_MAX;
    s_gain.blue = blue & WS2816_CURRENT_GAIN_MAX;
}

void ws2816_clear(void)
{
    ws2816_set_all(0U, 0U, 0U);
}

void ws2816_show(void)
{
    if (!s_initialized || s_driver == NULL)
    {
        return;
    }

    s_driver->transmit(s_pixels, s_led_count, s_brightness, &s_gain);
}

uint32_t ws2816_get_led_count(void)
{
    return s_led_count;
}
