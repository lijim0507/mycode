/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws281x.h"
#include "ws281x_port.h"

#include <string.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/**
 * @brief  WS281X 设备上下文
 */
typedef struct ws281x_dev {
    const ws281x_driver_t *driver;
    ws281x_dev_type_t      dev_type;
    uint8_t                bits_per_ch;
    uint8_t                bytes_per_ch;
    bool                   has_gain;
    uint32_t               num_leds;
    uint8_t                brightness;
    bool                   initialized;
    uint32_t               encode_buf_len;

    uint16_t               pixel_buf[WS281X_MAX_LEDS * WS281X_CH_PER_LED];
    uint8_t                encode_buf[WS281X_MAX_FRAME_SIZE];

    uint8_t                gain_g;
    uint8_t                gain_r;
    uint8_t                gain_b;

    uint32_t               breath_tick;
    uint16_t               rainbow_hue_offset;
    uint32_t               chase_position;
} ws281x_dev_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static void ws281x_encode(ws281x_handle_t dev, const uint16_t *pixels, uint8_t *output, uint32_t num_leds);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static ws281x_dev_t g_dev;
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
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 WS281X 驱动库，返回设备句柄
 * @param  num_leds LED 灯珠数量
 * @param  dev_type 设备类型 (WS281X_DEV_WS2812 或 WS281X_DEV_WS2816A)
 * @return 非NULL: 成功返回设备句柄, NULL: 失败
 */
ws281x_handle_t ws281x_init(uint32_t num_leds, ws281x_dev_type_t dev_type)
{
    const ws281x_driver_t *driver;

    if (num_leds == 0 || num_leds > WS281X_MAX_LEDS)
    {
        return NULL;
    }

    if (dev_type != WS281X_DEV_WS2812 && dev_type != WS281X_DEV_WS2816A)
    {
        return NULL;
    }

    driver = ws281x_port_get_driver();
    if (!driver || !driver->init || !driver->transmit)
    {
        return NULL;
    }

    if (g_dev.initialized)
    {
        ws281x_deinit(&g_dev);
    }

    g_dev.driver = driver;
    g_dev.dev_type = dev_type;
    g_dev.num_leds = num_leds;
    g_dev.brightness = 255;
    g_dev.gain_g = 31;
    g_dev.gain_r = 31;
    g_dev.gain_b = 31;

    if (dev_type == WS281X_DEV_WS2816A)
    {
        g_dev.bits_per_ch = 16;
        g_dev.bytes_per_ch = 2;
        g_dev.has_gain = true;
        g_dev.encode_buf_len = num_leds * WS281X_CH_PER_LED * 2 + 2;
    }
    else
    {
        g_dev.bits_per_ch = 8;
        g_dev.bytes_per_ch = 1;
        g_dev.has_gain = false;
        g_dev.encode_buf_len = num_leds * WS281X_CH_PER_LED;
    }

    memset(g_dev.pixel_buf, 0, sizeof(g_dev.pixel_buf));
    memset(g_dev.encode_buf, 0, sizeof(g_dev.encode_buf));
    g_dev.breath_tick = 0;
    g_dev.rainbow_hue_offset = 0;
    g_dev.chase_position = 0;

    if (driver->init() != 0)
    {
        return NULL;
    }

    g_dev.initialized = true;
    return &g_dev;
}

/**
 * @brief  反初始化，释放所有资源
 * @param  dev 设备句柄
 * @return 0: 成功, -1: 参数错误
 */
int ws281x_deinit(ws281x_handle_t dev)
{
    if (!dev || !dev->initialized)
    {
        return -1;
    }

    if (dev->driver && dev->driver->deinit)
    {
        dev->driver->deinit();
    }

    dev->driver = NULL;
    dev->num_leds = 0;
    dev->encode_buf_len = 0;
    dev->initialized = false;
    return 0;
}

/**
 * @brief  设置单颗灯珠 RGB 颜色
 * @param  dev   设备句柄
 * @param  index 灯珠索引
 * @param  r     红色分量
 * @param  g     绿色分量
 * @param  b     蓝色分量
 */
void ws281x_set_pixel(ws281x_handle_t dev, uint32_t index, ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b)
{
    if (!dev || !dev->initialized || index >= dev->num_leds)
    {
        return;
    }
    dev->pixel_buf[index * 3 + 0] = g;
    dev->pixel_buf[index * 3 + 1] = r;
    dev->pixel_buf[index * 3 + 2] = b;
}

/**
 * @brief  设置所有灯珠为同一颜色
 * @param  dev 设备句柄
 * @param  r   红色分量
 * @param  g   绿色分量
 * @param  b   蓝色分量
 */
void ws281x_set_all(ws281x_handle_t dev, ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b)
{
    if (!dev || !dev->initialized)
    {
        return;
    }

    for (uint32_t i = 0; i < dev->num_leds; i++)
    {
        dev->pixel_buf[i * 3 + 0] = g;
        dev->pixel_buf[i * 3 + 1] = r;
        dev->pixel_buf[i * 3 + 2] = b;
    }
}

/**
 * @brief  使用预定义颜色设置单颗灯珠
 * @param  dev   设备句柄
 * @param  index 灯珠索引
 * @param  color 预定义颜色枚举
 */
void ws281x_set_pixel_color(ws281x_handle_t dev, uint32_t index, ws281x_color_t color)
{
    uint8_t r8, g8, b8;
    ws281x_color_get_rgb(color, &r8, &g8, &b8);

    ws281x_pixel_t r, g, b;
    if (dev && dev->has_gain)
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

    ws281x_set_pixel(dev, index, r, g, b);
}

/**
 * @brief  使用预定义颜色设置所有灯珠
 * @param  dev   设备句柄
 * @param  color 预定义颜色枚举
 */
void ws281x_set_all_color(ws281x_handle_t dev, ws281x_color_t color)
{
    uint8_t r8, g8, b8;
    ws281x_color_get_rgb(color, &r8, &g8, &b8);

    ws281x_pixel_t r, g, b;
    if (dev && dev->has_gain)
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

    ws281x_set_all(dev, r, g, b);
}

/**
 * @brief  设置全局亮度
 * @param  dev         设备句柄
 * @param  brightness 亮度 (0~255)
 */
void ws281x_set_brightness(ws281x_handle_t dev, uint8_t brightness)
{
    if (!dev)
    {
        return;
    }
    dev->brightness = brightness;
}

/**
 * @brief  设置全局增益（仅 WS2816A 等支持增益的设备有效）
 * @param  dev    设备句柄
 * @param  gain_g 绿色增益 (5bit, 0~31)
 * @param  gain_r 红色增益 (5bit, 0~31)
 * @param  gain_b 蓝色增益 (5bit, 0~31)
 * @note   WS2812 设备调用此函数无效
 */
void ws281x_set_gain(ws281x_handle_t dev, uint8_t gain_g, uint8_t gain_r, uint8_t gain_b)
{
    if (!dev || !dev->has_gain)
    {
        return;
    }
    dev->gain_g = gain_g & 0x1F;
    dev->gain_r = gain_r & 0x1F;
    dev->gain_b = gain_b & 0x1F;
}

/**
 * @brief  获取 LED 数量
 * @param  dev 设备句柄
 * @return LED 灯珠数量
 */
uint32_t ws281x_get_num_leds(ws281x_handle_t dev)
{
    if (!dev)
    {
        return 0;
    }
    return dev->num_leds;
}

/**
 * @brief  熄灭所有灯珠
 * @param  dev 设备句柄
 */
void ws281x_clear(ws281x_handle_t dev)
{
    ws281x_set_all(dev, 0, 0, 0);
}

/**
 * @brief  推送像素数据到灯带（阻塞）
 * @param  dev 设备句柄
 * @note   内部先将 GRB 像素数据编码，再通过 port 层发送
 */
void ws281x_show(ws281x_handle_t dev)
{
    if (!dev || !dev->initialized || !dev->driver)
    {
        return;
    }

    if (dev->driver->is_busy && dev->driver->is_busy())
    {
        while (dev->driver->is_busy())
        {
        }
    }

    ws281x_encode(dev, dev->pixel_buf, dev->encode_buf, dev->num_leds);
    dev->driver->transmit(dev->encode_buf, dev->encode_buf_len);
}

/**
 * @brief  推送像素数据到灯带（非阻塞）
 * @param  dev 设备句柄
 * @return true: 发送已启动, false: 上次传输未完成或参数错误
 */
bool ws281x_show_async(ws281x_handle_t dev)
{
    if (!dev || !dev->initialized || !dev->driver)
    {
        return false;
    }

    if (dev->driver->is_busy && dev->driver->is_busy())
    {
        return false;
    }

    ws281x_encode(dev, dev->pixel_buf, dev->encode_buf, dev->num_leds);
    return (dev->driver->transmit(dev->encode_buf, dev->encode_buf_len) == 0);
}

/**
 * @brief  查询传输状态
 * @param  dev 设备句柄
 * @return true: 正在发送, false: 空闲或参数错误
 */
bool ws281x_is_busy(ws281x_handle_t dev)
{
    if (!dev || !dev->initialized || !dev->driver || !dev->driver->is_busy)
    {
        return false;
    }

    return (dev->driver->is_busy() != 0);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  WS281X 协议编码：像素数据 → 发送缓冲区
 * @param  dev      设备句柄
 * @param  pixels   原始像素缓冲区（GRB 格式）
 * @param  output   编码后输出缓冲区
 * @param  num_leds LED 数量
 */
static void ws281x_encode(ws281x_handle_t dev, const uint16_t *pixels, uint8_t *output, uint32_t num_leds)
{
    uint32_t out_idx = 0;
    uint32_t num_ch  = num_leds * WS281X_CH_PER_LED;

    if (dev->has_gain)
    {
        uint16_t gain_word = ((uint16_t)(dev->gain_g & 0x1F) << 11)
                           | ((uint16_t)(dev->gain_r & 0x1F) << 6)
                           | ((uint16_t)(dev->gain_b & 0x1F) << 1)
                           | 0U;
        output[out_idx++] = (uint8_t)(gain_word >> 8);
        output[out_idx++] = (uint8_t)(gain_word & 0xFF);
    }

    for (uint32_t i = 0; i < num_ch; i++)
    {
        uint16_t val = pixels[i];
        if (dev->brightness != 255)
        {
            val = (uint16_t)(((uint32_t)val * dev->brightness) / 255U);
        }

        if (dev->bits_per_ch == 16)
        {
            output[out_idx++] = (uint8_t)(val >> 8);
            output[out_idx++] = (uint8_t)(val & 0xFF);
        }
        else
        {
            output[out_idx++] = (uint8_t)(val & 0xFF);
        }
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/