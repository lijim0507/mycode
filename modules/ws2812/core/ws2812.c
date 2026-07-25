/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws2812.h"
#include "ws2812_port.h"
#include "ws2812_config.h"

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


/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static const ws2812_driver_t *g_driver;

static ws281x_pixel_t   g_pixel_buf[WS2812_MAX_LEDS * WS281X_CH_PER_LED];   //最大灯珠数 * 每颗灯珠的通道数，不代表实际使用的灯珠数量缓存
static uint16_t         g_buf_len;      //根据设置灯珠数量计算的缓冲区长度
static uint32_t         g_num_leds;
static bool             g_initialized;


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
    g_buf_len = g_num_leds * WS281X_BYTES_PER_LED ;

    memset(g_pixel_buf, 0, sizeof(g_pixel_buf));

    
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
    g_buf_len = 0;
    g_initialized = false;
    return 0;
}

/**
 * @brief  设置单颗灯珠 RGB 颜色
 * @param  index 灯珠索引
 * @param  r     红色分量
 * @param  g     绿色分量s
 * @param  b     蓝色分量
 * @param  brightness 亮度 (0.0~1.0)
 */
void ws2812_set_pixel(uint32_t index, ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b, float brightness)
{
    if (!g_initialized || index >= g_num_leds)
    {
        return;
    }
    g_pixel_buf[index * 3 + 0] = (ws281x_pixel_t)((float)g * brightness);
    g_pixel_buf[index * 3 + 1] = (ws281x_pixel_t)((float)r * brightness);
    g_pixel_buf[index * 3 + 2] = (ws281x_pixel_t)((float)b * brightness);
}

/**
 * @brief  设置所有灯珠为同一颜色
 * @param  r 红色分量
 * @param  g 绿色分量
 * @param  b 蓝色分量
 */
void ws2812_set_all(ws281x_pixel_t r, ws281x_pixel_t g, ws281x_pixel_t b, float brightness)
{
    if (!g_initialized)
    {
        return;
    }

    for (uint32_t i = 0; i < g_num_leds; i++)
    {
        ws2812_set_pixel(i, r, g, b, brightness);
    }
}

/**
 * @brief  使用预定义颜色设置单颗灯珠
 * @param  index 灯珠索引
 * @param  color 预定义颜色枚举
 */
void ws2812_set_pixel_color(uint32_t index, ws2812_color_t color, float brightness)
{
    ws281x_pixel_t r, g, b;
    ws2812_color_get_rgb(color, &r, &g, &b);
    ws2812_set_pixel(index, r, g, b, brightness);
}

/**
 * @brief  使用预定义颜色设置所有灯珠
 * @param  color 预定义颜色枚举
 */
void ws2812_set_all_color(ws2812_color_t color, float brightness)
{
    ws281x_pixel_t r, g, b;
    ws2812_color_get_rgb(color, &r, &g, &b);
    ws2812_set_all(r, g, b, brightness);
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
    ws2812_set_all(0, 0, 0, 1.0f);
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

    g_driver->transmit((uint8_t*)g_pixel_buf, g_buf_len);
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

    return (g_driver->transmit((uint8_t*)g_pixel_buf, g_buf_len) == 0);
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


/****************************************************************************/
/*								EOF											*/
/****************************************************************************/