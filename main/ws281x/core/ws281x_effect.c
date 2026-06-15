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

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions    						    */
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
void ws281x_effect_breath(ws281x_handle_t dev, uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms)
{
    uint32_t now;
    uint32_t half;
    uint32_t phase;
    uint8_t  factor;

    if (!dev || period_ms == 0)
    {
        return;
    }

    dev->breath_tick += 10;
    now = dev->breath_tick;
    half = period_ms / 2U;

    if (now < half)
    {
        phase = now;
    }
    else
    {
        phase = period_ms - now;
    }

    factor = (uint8_t)(((uint32_t)phase * 255U) / half);

    ws281x_pixel_t pr, pg, pb;
    uint8_t r8 = (uint8_t)(((uint16_t)r * factor) / 255U);
    uint8_t g8 = (uint8_t)(((uint16_t)g * factor) / 255U);
    uint8_t b8 = (uint8_t)(((uint16_t)b * factor) / 255U);

    if (dev->has_gain)
    {
        pr = ws281x_expand_8bit(r8);
        pg = ws281x_expand_8bit(g8);
        pb = ws281x_expand_8bit(b8);
    }
    else
    {
        pr = r8;
        pg = g8;
        pb = b8;
    }

    ws281x_set_all(dev, pr, pg, pb);
}

/**
 * @brief  彩虹循环效果
 * @param  dev         设备句柄
 * @param  speed       速度 (0~255)，越大越快
 * @param  brightness  亮度 (0~255)
 * @note   需要周期性调用，调用后需显式调用 ws281x_show()
 */
void ws281x_effect_rainbow(ws281x_handle_t dev, uint8_t speed, uint8_t brightness)
{
    uint32_t num_leds;
    uint32_t i;
    uint16_t hue;
    uint8_t r8, g8, b8;
    ws281x_pixel_t r, g, b;

    num_leds = ws281x_get_num_leds(dev);
    if (num_leds == 0)
    {
        return;
    }

    for (i = 0; i < num_leds; i++)
    {
        hue = (uint16_t)(dev->rainbow_hue_offset + (i * 360U / num_leds));
        ws281x_hsv2rgb(hue % 360, 255, brightness, &r8, &g8, &b8);

        if (dev->has_gain)
        {
            r = ws281x_expand_8bit(r8);
            g = ws281x_expand_8bit(g8);
            b = ws281x_expand_8bit(b8);
        }
        else
        {
            r = r8;
            g = g8;
            b = b8;
        }

        ws281x_set_pixel(dev, i, r, g, b);
    }

    dev->rainbow_hue_offset = (uint16_t)(dev->rainbow_hue_offset + speed);

    if (dev->rainbow_hue_offset >= 360)
    {
        dev->rainbow_hue_offset -= 360;
    }
}

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
void ws281x_effect_chase(ws281x_handle_t dev, uint8_t r, uint8_t g, uint8_t b, uint32_t speed_ms, uint8_t size)
{
    uint32_t num_leds;
    uint32_t i;

    (void)speed_ms;

    num_leds = ws281x_get_num_leds(dev);
    if (num_leds == 0)
    {
        return;
    }

    ws281x_clear(dev);

    for (i = 0; i < size; i++)
    {
        uint32_t idx = (dev->chase_position + i) % num_leds;
        uint8_t fade = (uint8_t)(255U - (i * 255U / (size > 1 ? size : 1)));
        uint8_t r8 = (uint8_t)(((uint16_t)r * fade) / 255U);
        uint8_t g8 = (uint8_t)(((uint16_t)g * fade) / 255U);
        uint8_t b8 = (uint8_t)(((uint16_t)b * fade) / 255U);

        ws281x_pixel_t pr, pg, pb;
        if (dev->has_gain)
        {
            pr = ws281x_expand_8bit(r8);
            pg = ws281x_expand_8bit(g8);
            pb = ws281x_expand_8bit(b8);
        }
        else
        {
            pr = r8;
            pg = g8;
            pb = b8;
        }

        ws281x_set_pixel(dev, idx, pr, pg, pb);
    }

    dev->chase_position = (dev->chase_position + 1) % num_leds;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/