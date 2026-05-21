/****************************************************************************/
/*							Includes									*/
/****************************************************************************/
#include "ws2812b.h"
/****************************************************************************/
/*							Macros										*/
/****************************************************************************/
/* LED 数量 */

/* ==========================================================
 * 时序参数 (软件循环迭代次数，非 CPU Cycles)
 * 
 * 重要：以下数值基于 480MHz 主频 + AC5 -O0 优化级别估算，
 *       实际时序受编译器优化、HAL_GPIO_WritePin 函数调用开销影响。
 *       建议首次使用时用示波器测量并调整，确保满足 WS2812B 规格：
 *         T0H: 220~380ns, T0L: 580~1600ns
 *         T1H: 580~1600ns, T1L: 580~1600ns
 *         RESET: > 50us (建议 > 80us)
 * ========================================================== */
#define WS2812B_T0H_DELAY    10U    /* 0码高电平延时迭代次数 */
#define WS2812B_T0L_DELAY    45U    /* 0码低电平延时迭代次数 */

#define WS2812B_T1H_DELAY    50U    /* 1码高电平延时迭代次数 */
#define WS2812B_T1L_DELAY    15U    /* 1码低电平延时迭代次数 */

#define WS2812B_RESET_DELAY  6000U  /* Reset 低电平延时迭代次数 (~>80us) */

/* 是否在中断关闭状态下发送 (推荐开启，防止任务切换/中断破坏时序) */
#define WS2812B_DISABLE_IRQ_DURING_SEND  1

/* 以下接口要根据不同平台做适配修改 */
#define WS2812B_GPIO_PORT    GPIOE
#define WS2812B_GPIO_PIN     GPIO_PIN_2

#define WS2812B_GPIO_OUTPUT(x)  do{HAL_GPIO_WritePin(WS2812B_GPIO_PORT, WS2812B_GPIO_PIN, x);}while(0)

/****************************************************************************/
/*							Typedefs										*/
/****************************************************************************/

/****************************************************************************/
/*					Prototypes Of Local Functions						*/
/****************************************************************************/
static void ws2812b_send_byte(uint8_t byte);
static void ws2812b_send_pixels(const uint8_t *rgb_data, uint16_t num_pixels);
static void ws2812b_color_to_rgb(led_color_t color, uint8_t *r, uint8_t *g, uint8_t *b);

/****************************************************************************/
/*						Global Variables								*/
/****************************************************************************/
ws2812b_t ws2812b_pixels[WS2812B_PIXELS_NUM];
static uint8_t s_brightness = 255;
static uint8_t s_send_buf[WS2812B_PIXELS_NUM * 3];

/****************************************************************************/
/*						Exported Functions    							*/
/****************************************************************************/

/**
 * @brief 
 * 
 */
void ws2812b_init(void)
{
    WS2812B_GPIO_OUTPUT(0);
}

/**
 * @brief 
 * 
 */
void ws2812b_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= WS2812B_PIXELS_NUM)
    {
        return;
    }
    ws2812b_pixels[index].r = r;
    ws2812b_pixels[index].g = g;
    ws2812b_pixels[index].b = b;
}

/**
 * @brief 
 * 
 */
void ws2812b_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < WS2812B_PIXELS_NUM; i++)
    {
        ws2812b_set_pixel(i, r, g, b);
    }
}

/**
 * @brief 
 * 
 */
void ws2812b_update(void)
{
    uint8_t i;

    /* 若亮度为 0，直接发送全零 (熄灭) */
    if (s_brightness == 0)
    {
        for (i = 0; i < (WS2812B_PIXELS_NUM * 3); i++)
        {
            s_send_buf[i] = 0;
        }
    }
    /* 若亮度为最大值 (255)，直接发送原始数据 */
    else if (s_brightness == 255)
    {
        for (i = 0; i < WS2812B_PIXELS_NUM; i++)
        {
            s_send_buf[i * 3 + 0] = ws2812b_pixels[i].g;
            s_send_buf[i * 3 + 1] = ws2812b_pixels[i].r;
            s_send_buf[i * 3 + 2] = ws2812b_pixels[i].b;
        }
    }
    /* 其他亮度：逐像素逐通道按内部 0~255 刻度缩放 */
    else
    {
        for (i = 0; i < WS2812B_PIXELS_NUM; i++)
        {
            s_send_buf[i * 3 + 0] = (uint8_t)(((uint16_t)ws2812b_pixels[i].g * s_brightness) / 255U);
            s_send_buf[i * 3 + 1] = (uint8_t)(((uint16_t)ws2812b_pixels[i].r * s_brightness) / 255U);
            s_send_buf[i * 3 + 2] = (uint8_t)(((uint16_t)ws2812b_pixels[i].b * s_brightness) / 255U);
        }
    }

    ws2812b_send_pixels(s_send_buf, WS2812B_PIXELS_NUM);
}

/**
 * @brief 
 * 
 */
void ws2812b_set_pixel_color(uint8_t index, led_color_t color)
{
    uint8_t r, g, b;

    if (color >= LED_COLOR_NUM)
    {
        color = LED_COLOR_OFF;
    }
    ws2812b_color_to_rgb(color, &r, &g, &b);
    ws2812b_set_pixel(index, r, g, b);
}

/**
 * @brief 
 * 
 */
void ws2812b_set_all_color(led_color_t color)
{
    uint8_t r, g, b;

    if (color >= LED_COLOR_NUM)
    {
        color = LED_COLOR_OFF;
    }
    ws2812b_color_to_rgb(color, &r, &g, &b);
    ws2812b_set_all(r, g, b);
}

/****************************************************************************/
/*						Static Functions    							*/
/****************************************************************************/


/*
 * 颜色枚举转 RGB 查找表
 */
static void ws2812b_color_to_rgb(led_color_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    switch (color)
    {
        case LED_COLOR_RED:
            *r = 0xFF; *g = 0x00; *b = 0x00; break;
        case LED_COLOR_GREEN:
            *r = 0x00; *g = 0xFF; *b = 0x00; break;
        case LED_COLOR_BLUE:
            *r = 0x00; *g = 0x00; *b = 0xFF; break;
        case LED_COLOR_WHITE:
            *r = 0xFF; *g = 0xFF; *b = 0xFF; break;
        case LED_COLOR_YELLOW:
            *r = 0xFF; *g = 0xFF; *b = 0x00; break;
        case LED_COLOR_CYAN:
            *r = 0x00; *g = 0xFF; *b = 0xFF; break;
        case LED_COLOR_MAGENTA:
            *r = 0xFF; *g = 0x00; *b = 0xFF; break;
        case LED_COLOR_ORANGE:
            *r = 0xFF; *g = 0x80; *b = 0x00; break;
        case LED_COLOR_PURPLE:
            *r = 0x80; *g = 0x00; *b = 0x80; break;
        case LED_COLOR_PINK:
            *r = 0xFF; *g = 0xC0; *b = 0xCB; break;
        case LED_COLOR_LIME:
            *r = 0x32; *g = 0xFF; *b = 0x32; break;
        case LED_COLOR_SKYBLUE:
            *r = 0x87; *g = 0xCE; *b = 0xEB; break;
        case LED_COLOR_GOLD:
            *r = 0xFF; *g = 0xD7; *b = 0x00; break;
        case LED_COLOR_SILVER:
            *r = 0xC0; *g = 0xC0; *b = 0xC0; break;
        case LED_COLOR_OFF:
        case LED_COLOR_BLACK:
        default:
            *r = 0x00; *g = 0x00; *b = 0x00; break;
    }
}

/*
 * 发送单个字节 (MSB first)
 * @param byte 待发送的字节
 * @note 使用 HAL_GPIO_WritePin 翻转电平，配合 ws2812b_delay 产生 WS2812B 时序
 */
static void ws2812b_send_byte(uint8_t byte)
{
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        if (byte & 0x80)
        {
            /* 发送逻辑 1 */
            WS2812B_GPIO_OUTPUT(1);
            ws2812b_delay(WS2812B_T1H_DELAY);
            WS2812B_GPIO_OUTPUT(0);
            ws2812b_delay(WS2812B_T1L_DELAY);
        }
        else
        {
            /* 发送逻辑 0 */
            WS2812B_GPIO_OUTPUT(1);
            ws2812b_delay(WS2812B_T0H_DELAY);
            WS2812B_GPIO_OUTPUT(0);
            ws2812b_delay(WS2812B_T0L_DELAY);
        }
        byte <<= 1;
    }
}

/**
 * @brief  发送像素数据到 WS2812B
 * @param  rgb_data     像素数据缓冲区，格式为 GRB GRB GRB... (每个像素 3 字节)
 * @param  num_pixels   像素数量
 * @note   本函数会短暂关闭中断以保证 WS2812B 时序，4 颗 LED 约占用 200us
 */
static void ws2812b_send_pixels(const uint8_t *rgb_data, uint16_t num_pixels)
{
#if WS2812B_DISABLE_IRQ_DURING_SEND
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
#endif

    for (uint16_t i = 0; i < num_pixels; i++)
    {
        ws2812b_send_byte(rgb_data[i * 3 + 0]); /* G */
        ws2812b_send_byte(rgb_data[i * 3 + 1]); /* R */
        ws2812b_send_byte(rgb_data[i * 3 + 2]); /* B */
    }

    /* Reset 脉冲：低电平保持 > 50us */
    WS2812B_GPIO_OUTPUT(0);
    ws2812b_delay(WS2812B_RESET_DELAY);

#if WS2812B_DISABLE_IRQ_DURING_SEND
    __set_PRIMASK(primask);
#endif
}

/****************************************************************************/
/*							EOF											*/
/****************************************************************************/
