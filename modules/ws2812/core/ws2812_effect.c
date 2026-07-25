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
 * @param  r          红色分量 (0~255)
 * @param  g          绿色分量 (0~255)
 * @param  b          蓝色分量 (0~255)
 * @param  period_ms  呼吸周期 (ms)
 * @note   需要周期性调用（如每 10ms），调用后需手动调用 ws2812_show()
 */
void ws2812_effect_breath(uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms)
{
    static uint32_t tick;
    uint32_t now;
    uint32_t half;
    uint32_t phase;
    uint8_t  factor;

    if (period_ms == 0) 
    {
        return;
    }

    /* 每次调用递增 10 个单位以模拟时间流逝 */
    tick = (tick + 10) % period_ms;
    now = tick;
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

    ws2812_set_all(
        (uint8_t)(((uint16_t)r * factor) / 255U),
        (uint8_t)(((uint16_t)g * factor) / 255U),
        (uint8_t)(((uint16_t)b * factor) / 255U), 1.0f
    );
}

/**
 * @brief  彩虹循环效果
 * @param  speed       速度 (0~255)，越大越快
 * @param  brightness  亮度 (0~255)
 * @note   需要周期性调用，调用后需手动调用 ws2812_show()
 */
void ws2812_effect_rainbow(uint8_t speed, uint8_t brightness)
{
    static uint16_t hue_offset;
    uint32_t num_leds;
    uint32_t i;
    uint16_t hue;
    ws281x_pixel_t r, g, b;

    num_leds = ws2812_get_num_leds();
    if (num_leds == 0)
    {
        return;
    }

    for (i = 0; i < num_leds; i++)
    {
        hue = (uint16_t)(hue_offset + (i * 360U / num_leds));
        ws2812_hsv2rgb(hue % 360, 255, brightness, &r, &g, &b);
        ws2812_set_pixel(i, r, g, b, 1.0f);
    }

    hue_offset = (uint16_t)(hue_offset + speed);

    if (hue_offset >= 360)
    {
        hue_offset -= 360;
    }
}

/**
 * @brief  跑马灯效果
 * @param  r         红色分量 (0~255)
 * @param  g         绿色分量 (0~255)
 * @param  b         蓝色分量 (0~255)
 * @param  speed_ms  移动间隔 (ms)
 * @param  size      亮灯组大小
 * @note   需要周期性调用，调用后需手动调用 ws2812_show()
 */
void ws2812_effect_chase(uint8_t r, uint8_t g, uint8_t b, uint32_t speed_ms, uint8_t size)
{
    static uint32_t last_tick;
    static uint32_t position;
    uint32_t num_leds;
    uint32_t i;

    num_leds = ws2812_get_num_leds();
    if (num_leds == 0) 
    {
        return;
    }

    /* 简化实现：每次调用移动一步，speed_ms 参数预留用于后续与系统时钟对接 */
    (void)speed_ms;
    (void)last_tick;

    ws2812_clear();

    for (i = 0; i < size; i++) 
    {
        uint32_t idx = (position + i) % num_leds;
        uint8_t fade = (uint8_t)(255U - (i * 255U / (size > 1 ? size : 1)));
        ws2812_set_pixel(idx,
            (uint8_t)(((uint16_t)r * fade) / 255U),
            (uint8_t)(((uint16_t)g * fade) / 255U),
            (uint8_t)(((uint16_t)b * fade) / 255U), 1.0f);
    }

    position = (position + 1) % num_leds;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
