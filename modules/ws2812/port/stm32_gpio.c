/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws2812_port.h"

#include <stddef.h>
#include <string.h>

/*
 * ===================================================================
 *  STM32 Bit-Bang 参考移植层 (REFERENCE IMPLEMENTATION)
 *
 *  本文件为 STM32 平台的参考实现，使用 GPIO 位翻转方式驱动 WS2812。
 *  当前项目目标平台为 ESP32-S3，本文件不参与编译，仅供移植参考。
 *
 *  STM32 上使用时:
 *    1. 在编译选项中定义 WS2812_STM32_GPIO_PORT / _PIN
 *    2. 将 esp32_rmt.c 替换为本文件参与编译
 *    3. 生产环境建议使用 PWM + DMA 方案以获得可靠的时序
 *
 *  警告: 位翻转方式会占用 100% CPU 资源，并在发送期间关闭全局中断。
 * ===================================================================
 */

/*
 * ---- 根据实际硬件修改以下 GPIO 定义 ----
 * 原实现使用 PE2 (GPIOE, GPIO_PIN_2)
 */
#ifndef WS2812_STM32_GPIO_PORT
#define WS2812_STM32_GPIO_PORT   GPIOE
#endif
#ifndef WS2812_STM32_GPIO_PIN
#define WS2812_STM32_GPIO_PIN    GPIO_PIN_2
#endif
#ifndef WS2812_STM32_CLK_HZ
#define WS2812_STM32_CLK_HZ      (SystemCoreClock)
#endif

/* ---- ARM CMSIS 内联函数 (Cortex-M 系列) ---- */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#include <arm_compat.h>
#else
#include <cmsis_gcc.h>
#endif

/* ---- STM32 HAL 库 ---- */
#include "stm32h7xx_hal.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/* 时序基准常数 (480MHz 主频 + AC5 -O0 优化下校准) */
#define REF_CLK_HZ        480000000U              /* 基准主频                  */
#define REF_T0H_DELAY     10U                     /* 0 码高电平循环次数        */
#define REF_T0L_DELAY     45U                     /* 0 码低电平循环次数        */
#define REF_T1H_DELAY     50U                     /* 1 码高电平循环次数        */
#define REF_T1L_DELAY     15U                     /* 1 码低电平循环次数        */
#define REF_RESET_DELAY   6000U                   /* RESET 低电平循环次数      */

/* GPIO 输出宏 (平台适配接口) */
#define GPIO_OUT(x) \
    HAL_GPIO_WritePin(WS2812_STM32_GPIO_PORT, WS2812_STM32_GPIO_PIN, (x))

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/**
 * @brief  STM32 位翻转驱动上下文
 */
typedef struct {
    uint32_t t0h;           /* 0 码高电平延时              */
    uint32_t t0l;           /* 0 码低电平延时              */
    uint32_t t1h;           /* 1 码高电平延时              */
    uint32_t t1l;           /* 1 码低电平延时              */
    uint32_t reset;         /* RESET 延时                  */
    bool     busy;          /* 发送忙标志                  */
} stm32_bitbang_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static void delay_loop(volatile uint32_t count);
static void send_byte(uint8_t byte);

static int stm32_bitbang_init(void);
static int stm32_bitbang_transmit(const uint8_t *data, uint32_t len);
static int stm32_bitbang_is_busy(void);
static int stm32_bitbang_deinit(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static stm32_bitbang_ctx_t g_stm32;

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  NOP 软件延时循环
 * @param  count 循环迭代次数
 * @note   使用 __NOP() 防止编译器优化，实际延时与主频和优化级别相关
 */
static void delay_loop(volatile uint32_t count)
{
    while (count--) {
        __NOP();
    }
}

/**
 * @brief  位翻转发送单个字节 (MSB First)
 * @param  byte 待发送字节
 */
static void send_byte(uint8_t byte)
{
    for (uint8_t bit = 0; bit < 8; bit++) 
    {
        if (byte & 0x80) {
            /* 发送逻辑 1 */
            GPIO_OUT(1);
            delay_loop(g_stm32.t1h);
            GPIO_OUT(0);
            delay_loop(g_stm32.t1l);
        } else {
            /* 发送逻辑 0 */
            GPIO_OUT(1);
            delay_loop(g_stm32.t0h);
            GPIO_OUT(0);
            delay_loop(g_stm32.t0l);
        }
        byte <<= 1;
    }
}

/**
 * @brief  STM32 位翻转硬件初始化
 * @return 0: 成功
 * @note   根据当前 SystemCoreClock 自动缩放时序参数
 */
static int stm32_bitbang_init(void)
{
    uint32_t sysclk;
    uint32_t scale;

    sysclk = WS2812_STM32_CLK_HZ;

    /* 计算频率缩放因子 (四舍五入) */
    scale = (REF_CLK_HZ + (sysclk / 2U)) / sysclk;
    if (scale == 0) {
        scale = 1;
    }

    /* 按当前主频缩放所有延时参数 */
    g_stm32.t0h   = (REF_T0H_DELAY   + (scale / 2U)) / scale;
    g_stm32.t0l   = (REF_T0L_DELAY   + (scale / 2U)) / scale;
    g_stm32.t1h   = (REF_T1H_DELAY   + (scale / 2U)) / scale;
    g_stm32.t1l   = (REF_T1L_DELAY   + (scale / 2U)) / scale;
    g_stm32.reset = (REF_RESET_DELAY + (scale / 2U)) / scale;

    /* 防止高频下延时为零 */
    if (g_stm32.t0h   == 0) g_stm32.t0h   = 1;
    if (g_stm32.t0l   == 0) g_stm32.t0l   = 1;
    if (g_stm32.t1h   == 0) g_stm32.t1h   = 1;
    if (g_stm32.t1l   == 0) g_stm32.t1l   = 1;
    if (g_stm32.reset == 0) g_stm32.reset = 1;

    GPIO_OUT(0);
    g_stm32.busy = false;

    return 0;
}

/**
 * @brief  位翻转发送编码后数据
 * @param  data 编码后的发送缓冲区
 * @param  len  数据长度
 * @return 0: 成功
 * @note   发送期间关闭全局中断以保证 WS2812 时序精度
 */
static int stm32_bitbang_transmit(const uint8_t *data, uint32_t len)
{
    uint32_t primask;

    g_stm32.busy = true;

    /* 关闭全局中断以保证时序 */
    primask = __get_PRIMASK();
    __disable_irq();

    for (uint32_t i = 0; i < len; i++) {
        send_byte(data[i]);
    }

    /* RESET: 保持低电平 (WS2812B > 50us, WS2816A > 280us) */
    GPIO_OUT(0);
    delay_loop(g_stm32.reset);

    __set_PRIMASK(primask);

    g_stm32.busy = false;
    return 0;
}

/**
 * @brief  查询发送是否完成
 * @return 1: 正在发送, 0: 空闲
 */
static int stm32_bitbang_is_busy(void)
{
    return g_stm32.busy ? 1 : 0;
}

/**
 * @brief  反初始化
 * @return 0: 成功
 */
static int stm32_bitbang_deinit(void)
{
    GPIO_OUT(0);
    memset(&g_stm32, 0, sizeof(g_stm32));
    return 0;
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  获取 STM32 位翻转驱动实例
 * @return ws2812_driver_t 结构体指针
 */
const ws2812_driver_t *ws2812_port_get_driver(void)
{
    static const ws2812_driver_t driver = {
        .init     = stm32_bitbang_init,
        .transmit = stm32_bitbang_transmit,
        .is_busy  = stm32_bitbang_is_busy,
        .deinit   = stm32_bitbang_deinit,
    };
    return &driver;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
