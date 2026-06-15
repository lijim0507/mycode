/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws2812.h"
#include "ws2812_port.h"

#include <string.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static void ws2812_encode(const ws281x_pixel_t *pixels, uint8_t *output, uint32_t num_leds);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static const ws2812_driver_t *g_driver;
static ws281x_pixel_t  g_pixel_buf[WS2812_MAX_LEDS * WS281X_CH_PER_LED];
static uint8_t         g_encode_buf[WS2812_MAX_LEDS * WS281X_BYTES_PER_LED + WS281X_FRAME_HEADER_BYTES];
static uint32_t        g_num_leds;
static uint8_t         g_brightness = 255;
static bool            g_initialized;
static uint32_t        g_encode_buf_len;

#if WS281X_HAS_GAIN
static ws281x_gain_t   g_gain = {31, 31, 31};
#endif

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 WS2812 驱动库
 * @param  num_leds LED 灯珠数量
 * @return 0: 成功, -1: 参数错误, -3: 硬件初始化失败
 */
int ws2812_init(uint32_t num_leds)
{
    if (num_leds == 0 || num_leds > WS2812_MAX_LEDS)
    {
        return -1;
    }

    g_driver = ws2812_port_get_driver();
    if (!g_driver || !g_driver->init || !g_driver->transmit)
    {
        return -1;
    }

    if (g_initialized)
    {
        ws2812_deinit();
    }

    g_num_leds = num_leds;
    g_brightness = 255;
    g_encode_buf_len = num_leds * WS281X_BYTES_PER_LED + WS281X_FRAME_HEADER_BYTES;

    memset(g_pixel_buf, 0, sizeof(g_pixel_buf));
    memset(g_encode_buf, 0, sizeof(g_encode_buf));

    if (g_driver->init() != 0)
    {
        return -3;
    }

    g_initialized = true;
    return 0;
}

/**
 * @brief  反初始化，释放所有资源
 * @return 0: 成功
 */
int ws2812_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    if (g_driver && g_driver->deinit)
    {
        g_driver->deinit();
    }

    g_driver = NULL;
    g_num_leds = 0;
    g_encode_buf_len = 0;
    g_initialized = false;
    return 0;
}

/**
 * @brief  设置单颗灯珠 RGB 颜色
 * @param  index 灯珠索引
 * @param  r     红色分量
 * @param  g     绿色分量
 * @param  b     蓝色分量
 */
void ws2812_set_pixel(uint32_t index, ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b)
{
    if (!g_initialized || index >= g_num_leds)
    {
        return;
    }
    g_pixel_buf[index * 3 + 0] = g;
    g_pixel_buf[index * 3 + 1] = r;
    g_pixel_buf[index * 3 + 2] = b;
}

/**
 * @brief  设置所有灯珠为同一颜色
 * @param  r 红色分量
 * @param  g 绿色分量
 * @param  b 蓝色分量
 */
void ws2812_set_all(ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b)
{
    if (!g_initialized)
    {
        return;
    }

    for (uint32_t i = 0; i < g_num_leds; i++)
    {
        g_pixel_buf[i * 3 + 0] = g;
        g_pixel_buf[i * 3 + 1] = r;
        g_pixel_buf[i * 3 + 2] = b;
    }
}

/**
 * @brief  使用预定义颜色设置单颗灯珠
 * @param  index 灯珠索引
 * @param  color 预定义颜色枚举
 */
void ws2812_set_pixel_color(uint32_t index, ws2812_color_t color)
{
    ws281x_pixel_t r, g, b;
    ws2812_color_get_rgb(color, &r, &g, &b);
    ws2812_set_pixel(index, r, g, b);
}

/**
 * @brief  使用预定义颜色设置所有灯珠
 * @param  color 预定义颜色枚举
 */
void ws2812_set_all_color(ws2812_color_t color)
{
    ws281x_pixel_t r, g, b;
    ws2812_color_get_rgb(color, &r, &g, &b);
    ws2812_set_all(r, g, b);
}

/**
 * @brief  设置全局亮度
 * @param  brightness 亮度 (0~255)
 */
void ws2812_set_brightness(uint8_t brightness)
{
    g_brightness = brightness;
}

/**
 * @brief  获取 LED 数量
 * @return LED 灯珠数量
 */
uint32_t ws2812_get_num_leds(void)
{
    return g_num_leds;
}

/**
 * @brief  熄灭所有灯珠
 */
void ws2812_clear(void)
{
    ws2812_set_all(0, 0, 0);
}

/**
 * @brief  推送像素数据到灯带（阻塞）
 * @note   内部先将 GRB 像素数据编码，再通过 port 层发送
 */
void ws2812_show(void)
{
    if (!g_initialized || !g_driver)
    {
        return;
    }

    if (g_driver->is_busy && g_driver->is_busy())
    {
        while (g_driver->is_busy())
        {
        }
    }

    ws2812_encode(g_pixel_buf, g_encode_buf, g_num_leds);
    g_driver->transmit(g_encode_buf, g_encode_buf_len);
}

/**
 * @brief  推送像素数据到灯带（非阻塞）
 * @return true: 发送已启动, false: 上次传输未完成
 */
bool ws2812_show_async(void)
{
    if (!g_initialized || !g_driver)
    {
        return false;
    }

    if (g_driver->is_busy && g_driver->is_busy())
    {
        return false;
    }

    ws2812_encode(g_pixel_buf, g_encode_buf, g_num_leds);
    return (g_driver->transmit(g_encode_buf, g_encode_buf_len) == 0);
}

/**
 * @brief  查询传输状态
 * @return true: 正在发送, false: 空闲
 */
bool ws2812_is_busy(void)
{
    if (!g_initialized || !g_driver || !g_driver->is_busy)
    {
        return false;
    }

    return (g_driver->is_busy() != 0);
}

#if WS281X_HAS_GAIN
/**
 * @brief  设置全局增益（仅 WS2816A 等支持增益的设备有效）
 * @param  gain_g 绿色增益 (5bit, 0~31)
 * @param  gain_r 红色增益 (5bit, 0~31)
 * @param  gain_b 蓝色增益 (5bit, 0~31)
 */
void ws2812_set_gain(uint8_t gain_g, uint8_t gain_r, uint8_t gain_b)
{
    g_gain.gain_g = gain_g & 0x1F;
    g_gain.gain_r = gain_r & 0x1F;
    g_gain.gain_b = gain_b & 0x1F;
}
#endif

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  WS2812 协议编码：像素数据 → 发送缓冲区
 * @param  pixels    原始像素缓冲区（GRB 格式）
 * @param  output    编码后输出缓冲区
 * @param  num_leds  LED 数量
 */
static void ws2812_encode(const ws281x_pixel_t *pixels, uint8_t *output, uint32_t num_leds)
{
    uint32_t out_idx = 0;
    uint32_t num_ch  = num_leds * WS281X_CH_PER_LED;

#if WS281X_HAS_GAIN
    uint16_t gain_word = ((uint16_t)(g_gain.gain_g & 0x1F) << 11)
                       | ((uint16_t)(g_gain.gain_r & 0x1F) << 6)
                       | ((uint16_t)(g_gain.gain_b & 0x1F) << 1)
                       | 0U;
    output[out_idx++] = (uint8_t)(gain_word >> 8);
    output[out_idx++] = (uint8_t)(gain_word & 0xFF);
#endif

    for (uint32_t i = 0; i < num_ch; i++)
    {
        ws281x_pixel_t val = pixels[i];
        if (g_brightness != 255)
        {
            val = (ws281x_pixel_t)(((uint32_t)val * g_brightness) / 255U);
        }
#if WS281X_BITS_PER_CH == 16
        output[out_idx++] = (uint8_t)(val >> 8);
        output[out_idx++] = (uint8_t)(val & 0xFF);
#else
        output[out_idx++] = (uint8_t)val;
#endif
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/