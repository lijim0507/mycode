/****************************************************************************/
/*							Includes									*/
/****************************************************************************/
#include "ws2812b.h"
/****************************************************************************/
/*							Macros										*/
/****************************************************************************/


/* ==========================================================
 * 时序参数 (软件循环迭代次数，非 CPU Cycles)
 *
 * 重要：以下基准值基于 480MHz 主频 + AC5 -O0 优化级别估算，
 *       实际时序受编译器优化、HAL_GPIO_WritePin 函数调用开销影响。
 *       建议首次使用时用示波器测量并调整，确保满足 WS2812B 规格：
 *         T0H: 220~380ns, T0L: 580~1600ns
 *         T1H: 580~1600ns, T1L: 580~1600ns
 *         RESET: > 50us (建议 > 80us)
 *
 * 实际运行值在 ws2812b_init() 中根据 SystemCoreClock 自动缩放。
 * ========================================================== */
#define WS2812B_REF_CLK_HZ      480000000U  /* 基准主频 */

#define WS2812B_REF_T0H_DELAY   10U         /* 480MHz 下 0码高电平延时迭代次数 */
#define WS2812B_REF_T0L_DELAY   45U         /* 480MHz 下 0码低电平延时迭代次数 */
#define WS2812B_REF_T1H_DELAY   50U         /* 480MHz 下 1码高电平延时迭代次数 */
#define WS2812B_REF_T1L_DELAY   15U         /* 480MHz 下 1码低电平延时迭代次数 */
#define WS2812B_REF_RESET_DELAY 6000U       /* 480MHz 下 Reset 低电平延时迭代次数 */

/* 运行时根据主频计算的实际延时参数 */
static uint32_t ws2812b_t0h_delay;
static uint32_t ws2812b_t0l_delay;
static uint32_t ws2812b_t1h_delay;
static uint32_t ws2812b_t1l_delay;
static uint32_t ws2812b_reset_delay;

/* 是否在中断关闭状态下发送 (推荐开启，防止任务切换/中断破坏时序) */
#define WS2812B_DISABLE_IRQ_DURING_SEND  1

/* 以下接口要根据不同平台做适配修改 */
#define WS2812B_GPIO_PORT    GPIOE
#define WS2812B_GPIO_PIN     GPIO_PIN_2

#define WS2812B_GPIO_OUTPUT(x)  do{HAL_GPIO_WritePin(WS2812B_GPIO_PORT, WS2812B_GPIO_PIN, x);}while(0)

/****************************************************************************/
/*							Typedefs										*/
/****************************************************************************/
typedef struct _ws2812b_t
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
}ws2812b_t;
/****************************************************************************/
/*					Prototypes Of Local Functions						*/
/****************************************************************************/
static void ws2812b_delay(uint32_t time);
static void ws2812b_send_byte(uint8_t byte);
static void ws2812b_send_pixels(const uint8_t *rgb_data, uint16_t num_pixels);

static void ws2812b_set_pixel(uint8_t index, uint8_t g, uint8_t r, uint8_t b);
static void ws2812b_set_all_pixels(uint8_t g, uint8_t r, uint8_t b);

static void ws2812b_color_to_grb(led_color_t color, uint8_t *g, uint8_t *r, uint8_t *b);

/****************************************************************************/
/*						Global Variables								*/
/****************************************************************************/
ws2812b_t ws2812b_pixels[WS2812B_PIXELS_NUM];
uint8_t brightness = 255;


/****************************************************************************/
/*						Exported Functions    							*/
/****************************************************************************/

/**
 * @brief 
 * 
 */
void ws2812b_init(void)
{
    uint32_t sysclk = SystemCoreClock;

    /* 先计算缩放因子，避免 32 位乘法溢出 */
    uint32_t scale = (WS2812B_REF_CLK_HZ + (sysclk / 2U)) / sysclk;
    if (scale == 0)
    {
        scale = 1;
    }

    /* 根据当前主频按比例缩放延时参数 (四舍五入) */
    ws2812b_t0h_delay  = (WS2812B_REF_T0H_DELAY   + (scale / 2U)) / scale;
    ws2812b_t0l_delay  = (WS2812B_REF_T0L_DELAY   + (scale / 2U)) / scale;
    ws2812b_t1h_delay  = (WS2812B_REF_T1H_DELAY   + (scale / 2U)) / scale;
    ws2812b_t1l_delay  = (WS2812B_REF_T1L_DELAY   + (scale / 2U)) / scale;
    ws2812b_reset_delay = (WS2812B_REF_RESET_DELAY + (scale / 2U)) / scale;

    /* 确保至少为 1，防止除零或高频下延时丢失 */
    if (ws2812b_t0h_delay  == 0) ws2812b_t0h_delay  = 1;
    if (ws2812b_t0l_delay  == 0) ws2812b_t0l_delay  = 1;
    if (ws2812b_t1h_delay  == 0) ws2812b_t1h_delay  = 1;
    if (ws2812b_t1l_delay  == 0) ws2812b_t1l_delay  = 1;
    if (ws2812b_reset_delay == 0) ws2812b_reset_delay = 1;

    WS2812B_GPIO_OUTPUT(0);
}


/**
 * @brief 
 * 
 */
void ws2812b_set_brightness(float percent)
{
    if(percent>1)
    {   
        percent = 1.0f;
    }

    if(percent<0)
    {
        percent = 0.0f;
    }
    brightness = (uint8_t)(255.0f * percent);
}

void ws2812b_update(void)
{
    uint8_t pixels_send_buf[WS2812B_PIXELS_NUM * 3];
    uint8_t i;

    for (i = 0; i < WS2812B_PIXELS_NUM; i++)
    {
        pixels_send_buf[i * 3 + 0] = (uint8_t)(((uint16_t)ws2812b_pixels[i].g * brightness) / 255U);
        pixels_send_buf[i * 3 + 1] = (uint8_t)(((uint16_t)ws2812b_pixels[i].r * brightness) / 255U);
        pixels_send_buf[i * 3 + 2] = (uint8_t)(((uint16_t)ws2812b_pixels[i].b * brightness) / 255U);
    }

    ws2812b_send_pixels(pixels_send_buf, WS2812B_PIXELS_NUM);
}

/**
 * @brief 
 * 
 */
void ws2812b_set_pixel_color(uint8_t index, led_color_t color)
{
    uint8_t g, r, b;

    if (color >= LED_COLOR_NUM)
    {
        color = LED_COLOR_OFF;
    }
    ws2812b_color_to_grb(color, &g, &r, &b);
    ws2812b_set_pixel(index, g, r, b);
}

/**
 * @brief 
 * 
 */
void ws2812b_set_all_pixel_color(led_color_t color)
{
    uint8_t g, r, b;

    if (color >= LED_COLOR_NUM)
    {
        color = LED_COLOR_OFF;
    }
    ws2812b_color_to_grb(color, &g, &r, &b);
    ws2812b_set_all_pixels(g, r, b);
}

/****************************************************************************/
/*						Static Functions    							*/
/****************************************************************************/
/**
 * @brief  软件忙等待延时函数
 * @param  count  循环迭代次数
 * @note   使用 __NOP() 防止编译器优化，实际延时与主频、优化级别相关
 */
static void ws2812b_delay(volatile uint32_t count)
{
    while (count--)
    {
        __NOP();
    }
}

/**
 * @brief 颜色枚举转 WS2812B 标准 GRB 格式
 * @param color 颜色枚举
 * @param g 绿色输出
 * @param r 红色输出
 * @param b 蓝色输出
 */
static void ws2812b_color_to_grb(led_color_t color, uint8_t *g, uint8_t *r, uint8_t *b)
{
    switch (color)
    {
        case LED_COLOR_RED:        *g = 0x00; *r = 0xFF; *b = 0x00; break;
        case LED_COLOR_GREEN:      *g = 0xFF; *r = 0x00; *b = 0x00; break;
        case LED_COLOR_BLUE:       *g = 0x00; *r = 0x00; *b = 0xFF; break;
        case LED_COLOR_WHITE:      *g = 0xFF; *r = 0xFF; *b = 0xFF; break;
        case LED_COLOR_YELLOW:     *g = 0xFF; *r = 0xFF; *b = 0x00; break;
        case LED_COLOR_ORANGE:     *g = 0x80; *r = 0xFF; *b = 0x00; break;
        case LED_COLOR_PURPLE:     *g = 0x00; *r = 0xFF; *b = 0xFF; break;
        case LED_COLOR_PINK:       *g = 0xC0; *r = 0xFF; *b = 0xCB; break;
        case LED_COLOR_SKYBLUE:    *g = 0xCE; *r = 0x87; *b = 0xEB; break;
        case LED_COLOR_GOLD:       *g = 0xD7; *r = 0xFF; *b = 0x00; break;
        case LED_COLOR_SILVER:     *g = 0xC0; *r = 0xC0; *b = 0xC0; break;
        
        case LED_COLOR_OFF:
        case LED_COLOR_BLACK:
        default:
            *g = 0x00; *r = 0x00; *b = 0x00; break;
    }
}

/**
 * @brief 
 * 
 */
static void ws2812b_set_all_pixels(uint8_t g, uint8_t r, uint8_t b)
{
    for (uint8_t i = 0; i < WS2812B_PIXELS_NUM; i++)
    {
        ws2812b_set_pixel(i, g, r, b);
    }
}


/**
 * @brief 
 * 
 */
static void ws2812b_set_pixel(uint8_t index, uint8_t g, uint8_t r, uint8_t b)
{
    if (index >= WS2812B_PIXELS_NUM)
    {
        return;
    }
    ws2812b_pixels[index].g = g;
    ws2812b_pixels[index].r = r;
    ws2812b_pixels[index].b = b;
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
    ws2812b_delay(ws2812b_reset_delay);

#if WS2812B_DISABLE_IRQ_DURING_SEND
    __set_PRIMASK(primask);
#endif
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
            ws2812b_delay(ws2812b_t1h_delay);
            WS2812B_GPIO_OUTPUT(0);
            ws2812b_delay(ws2812b_t1l_delay);
        }
        else
        {
            /* 发送逻辑 0 */
            WS2812B_GPIO_OUTPUT(1);
            ws2812b_delay(ws2812b_t0h_delay);
            WS2812B_GPIO_OUTPUT(0);
            ws2812b_delay(ws2812b_t0l_delay);
        }
        byte <<= 1;
    }
}

/****************************************************************************/
/*							EOF											*/
/****************************************************************************/
