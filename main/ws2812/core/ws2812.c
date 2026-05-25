/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws2812.h"
#include "ws2812_port.h"

#include <string.h>
#include <stdlib.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static void ws2812_encode(const uint8_t *pixels, uint8_t *output, uint32_t num_leds);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static const ws2812_driver_t *g_driver;
static uint8_t  *g_pixel_buf;       /* 像素缓冲区：GRB GRB GRB ...           */
static uint8_t  *g_encode_buf;      /* 编码后发送缓冲区                       */
static uint32_t  g_num_leds;
static uint8_t   g_brightness = 255;
static bool      g_initialized;
static uint32_t  g_encode_buf_len;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 WS2812 驱动库
 * @param  driver   平台驱动指针
 * @param  port_cfg 平台相关配置，传 NULL 使用默认值
 * @param  num_leds LED 灯珠数量
 * @return 0: 成功, -1: 参数错误, -2: 内存不足, -3: 硬件初始化失败
 */
int ws2812_init(const ws2812_driver_t *driver, void *port_cfg, uint32_t num_leds)
{
    if (!driver || !driver->init || !driver->transmit || num_leds == 0) 
    {
        return -1;
    }

    if (g_initialized) 
    {
        ws2812_deinit();
    }

    g_num_leds = num_leds;
    g_brightness = 255;

    g_pixel_buf = (uint8_t *)calloc(num_leds * 3, 1);
    g_encode_buf_len = num_leds * 3;
    g_encode_buf = (uint8_t *)malloc(g_encode_buf_len);

    if (!g_pixel_buf || !g_encode_buf) 
    {
        free(g_pixel_buf);
        free(g_encode_buf);
        g_pixel_buf = NULL;
        g_encode_buf = NULL;
        return -2;
    }

    if (driver->init(port_cfg) != 0) 
    {
        free(g_pixel_buf);
        free(g_encode_buf);
        g_pixel_buf = NULL;
        g_encode_buf = NULL;
        return -3;
    }

    g_driver = driver;
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

    free(g_pixel_buf);
    free(g_encode_buf);
    g_pixel_buf = NULL;
    g_encode_buf = NULL;
    g_driver = NULL;
    g_num_leds = 0;
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
void ws2812_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
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
void ws2812_set_all(uint8_t r, uint8_t g, uint8_t b)
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
    uint8_t r, g, b;
    ws2812_color_get_rgb(color, &r, &g, &b);
    ws2812_set_pixel(index, r, g, b);
}

/**
 * @brief  使用预定义颜色设置所有灯珠
 * @param  color 预定义颜色枚举
 */
void ws2812_set_all_color(ws2812_color_t color)
{
    uint8_t r, g, b;
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
            /* 忙等待直到上次传输完成 */
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

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  WS2812 协议编码：像素数据 → 发送缓冲区
 * @param  pixels    原始像素缓冲区（GRB 格式）
 * @param  output    编码后输出缓冲区
 * @param  num_leds  LED 数量
 * @note   当前仅做亮度缩放，未来可扩展支持不同协议编码（如 APA102、SK6812）
 */
static void ws2812_encode(const uint8_t *pixels, uint8_t *output, uint32_t num_leds)
{
    if (g_brightness == 255) 
    {
        memcpy(output, pixels, num_leds * 3);
    } 
    else
    {
        for (uint32_t i = 0; i < num_leds * 3; i++) 
        {
            output[i] = (uint8_t)(((uint16_t)pixels[i] * g_brightness) / 255U);
        }
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
