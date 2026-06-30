/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws281x_port.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/*
 * ===================================================================
 *  STM32 Timer + DMA 移植层 (DOUBLE-BUFFER REFILL)
 *
 *  使用固定大小的 DMA ping-pong buffer + 循环 DMA + 半/全完成
 *  中断方式驱动 WS281X 灯带。每 bit 原始数据展开为 1 word 波形，
 *  但 DMA buffer 尺寸不再与 LED 数量成正比，大幅节省 RAM。
 *
 *  原理：
 *    定时器 PWM 输出固定周期 (1.25us) 方波。DMA 循环模式下，每
 *    传输完半区触发 HT/TC 中断，ISR 内将 encode_buf 的下批 bit
 *    展开填入刚发完的半区。
 *
 *    所有 bit 发送完毕后转入 reset 阶段：往后 3 轮中断持续填零，
 *    确保至少 HALF_WORDS 个零 word (×1.25us) 已传输，满足 >50us
 *    的 RESET 时序，随后主动停止 DMA。
 *
 *  STM32 上使用时：
 *    1. 在项目编译选项中按需重定义下方各宏 (TIM/GPIO/LED/DMA)
 *    2. 替换 esp32_rmt.c 为本文件参与编译
 *    3. CubeMX 需预先配置 DMA CIRCULAR 模式：
 *       - 定时器通道 "PWM Generation CHx"
 *       - DMA MemToPeriph 关联该定时器 Update 请求，
 *         Mode: Circular, Data Width: Half Word
 *       - GPIO 复用功能 + 时钟使能
 * ===================================================================
 */

/* ---- ARM CMSIS 内联函数 (Cortex-M 系列) ---- */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
//#include <arm_compat.h>
#else
#include <cmsis_gcc.h>
#endif

/* ---- STM32 HAL 库 ---- */
#include "stm32h7xx_hal.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/*
 * ---- 硬件引脚 & 定时器宏 ----
 */
#ifndef WS281X_STM32_TIM_HANDLE
extern TIM_HandleTypeDef htim4;
#define WS281X_STM32_TIM_HANDLE     (&htim4)
#endif

#ifndef WS281X_STM32_TIM_CHANNEL
#define WS281X_STM32_TIM_CHANNEL    TIM_CHANNEL_2
#endif

#ifndef WS281X_STM32_TIM_CLK_PSC
#define WS281X_STM32_TIM_CLK_PSC    2
#endif

#ifndef WS281X_STM32_TIM_CLK_HZ
#define WS281X_STM32_TIM_CLK_HZ     (240 * 1000000U / WS281X_STM32_TIM_CLK_PSC)
#endif

#ifndef WS281X_STM32_GPIO_PORT
#define WS281X_STM32_GPIO_PORT      GPIOD
#endif

#ifndef WS281X_STM32_GPIO_PIN
#define WS281X_STM32_GPIO_PIN       GPIO_PIN_13
#endif

#ifndef WS281X_STM32_GPIO_AF
#define WS281X_STM32_GPIO_AF        GPIO_AF2_TIM4
#endif

/**
 * @brief  WS281X 时序参数 (固定值)
 */
#define WS281X_BIT_PERIOD_NS      1250U
#define WS281X_T0H_NS              350U
#define WS281X_T1H_NS              850U

/**
 * @brief  DMA 双缓冲半区大小
 *        64 words 半区 = 80us > 50us RESET 最短要求
 *        256 words 半区 = 320us，给中断处理留出充足余量
 */
#ifndef WS281X_DMA_HALF_WORDS
#define WS281X_DMA_HALF_WORDS     256U
#endif

#if WS281X_DMA_HALF_WORDS < 64U
#error "WS281X_DMA_HALF_WORDS must be >= 64 for RESET timing (64 × 1.25us = 80us > 50us)"
#endif

#define WS281X_DMA_BUF_WORDS      (WS281X_DMA_HALF_WORDS * 2U)

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  STM32 Timer+DMA 驱动上下文
 */
typedef struct
{
    TIM_HandleTypeDef *htim;
    uint32_t           tim_channel;
    uint32_t           code_0;
    uint32_t           code_1;
    uint32_t           arr;
    uint16_t          *dma_buf;
    volatile bool      busy;
} stm32_timer_dma_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int stm32_timer_dma_init(void);
static int stm32_timer_dma_transmit(const uint8_t *data, uint32_t len);
static int stm32_timer_dma_is_busy(void);
static int stm32_timer_dma_deinit(void);
static int fill_half(uint16_t *half, uint32_t count);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static stm32_timer_dma_ctx_t g_stm32;
static uint16_t g_dma_buf[WS281X_DMA_BUF_WORDS];

static const uint8_t *g_src_ptr;
static uint32_t       g_src_remaining;
static uint8_t        g_bit_pos;
static uint8_t        g_reset_cnt;

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  将编码数据逐 bit 展开填入 DMA 半区
 * @param  half  半区起始地址
 * @param  count 半区 word 数
 * @return 1: 仍有未发送数据, 0: 全部 encode 数据已处理完毕
 */
static int fill_half(uint16_t *half, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (g_src_remaining == 0U)
        {
            half[i] = 0U;
        }
        else
        {
            if ((*g_src_ptr) & (0x80U >> g_bit_pos))
            {
                half[i] = (uint16_t)g_stm32.code_1;
            }
            else
            {
                half[i] = (uint16_t)g_stm32.code_0;
            }

            g_bit_pos++;
            if (g_bit_pos >= 8U)
            {
                g_bit_pos = 0U;
                g_src_ptr++;
                g_src_remaining--;
            }
        }
    }

    return (g_src_remaining > 0U) ? 1 : 0;
}

/**
 * @brief  清理指定地址范围的 D-Cache
 * @param  addr 起始地址 (自动向下对齐到 32 字节边界)
 * @param  size 字节数
 */
static void dcache_clean(void *addr, uint32_t size)
{
    uint32_t a = (uintptr_t)addr;
    uint32_t align = a & 0x1FU;

    SCB_CleanDCache_by_Addr((uint32_t *)(a & ~0x1FU), size + 32U + align);
}

/**
 * @brief  定时器 + DMA 硬件初始化
 * @return 0: 成功, -1: 时钟过低导致时序不可实现
 */
static int stm32_timer_dma_init(void)
{
    TIM_OC_InitTypeDef sConfigOC;
    GPIO_InitTypeDef gpio_init;
    TIM_HandleTypeDef *htim;
    uint32_t ticks_per_bit;
    uint32_t ticks_t0h;
    uint32_t ticks_t1h;

    memset(&g_stm32, 0, sizeof(g_stm32));

    ticks_per_bit = (uint32_t)(((uint64_t)WS281X_STM32_TIM_CLK_HZ * WS281X_BIT_PERIOD_NS) / 1000000000ULL);
    ticks_t0h     = (uint32_t)(((uint64_t)WS281X_STM32_TIM_CLK_HZ * WS281X_T0H_NS) / 1000000000ULL);
    ticks_t1h     = (uint32_t)(((uint64_t)WS281X_STM32_TIM_CLK_HZ * WS281X_T1H_NS) / 1000000000ULL);

    if (ticks_per_bit < 2 || ticks_t0h == 0 || ticks_t1h == 0 || ticks_t1h >= ticks_per_bit)
    {
        return -1;
    }

    g_stm32.arr         = ticks_per_bit - 1U;
    g_stm32.code_0      = ticks_t0h;
    g_stm32.code_1      = ticks_t1h;
    g_stm32.htim        = WS281X_STM32_TIM_HANDLE;
    g_stm32.tim_channel = WS281X_STM32_TIM_CHANNEL;
    g_stm32.dma_buf     = g_dma_buf;

    htim = WS281X_STM32_TIM_HANDLE;

    htim->Init.Prescaler         = WS281X_STM32_TIM_CLK_PSC - 1U;
    htim->Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim->Init.Period            = g_stm32.arr;
    htim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(htim);

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0U;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, g_stm32.tim_channel);

    gpio_init.Pin       = WS281X_STM32_GPIO_PIN;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = WS281X_STM32_GPIO_AF;
    HAL_GPIO_Init(WS281X_STM32_GPIO_PORT, &gpio_init);

    g_stm32.busy = false;

    return 0;
}

/**
 * @brief  将编码后的数据通过 Timer+DMA 发送到灯带
 * @param  data 编码后的发送缓冲区
 * @param  len  数据长度 (字节)
 * @return 0: 成功, -1: 上次传输未完成或参数错误
 */
static int stm32_timer_dma_transmit(const uint8_t *data, uint32_t len)
{
    if (len == 0U || g_stm32.busy)
    {
        return -1;
    }

    g_src_ptr       = data;
    g_src_remaining = len;
    g_bit_pos       = 0U;
    g_reset_cnt     = 0U;

    fill_half(g_dma_buf, WS281X_DMA_HALF_WORDS);
    fill_half(g_dma_buf + WS281X_DMA_HALF_WORDS, WS281X_DMA_HALF_WORDS);

    dcache_clean(g_dma_buf, WS281X_DMA_BUF_WORDS * sizeof(uint16_t));

    g_stm32.busy = true;

    HAL_TIM_PWM_Start_DMA(g_stm32.htim, g_stm32.tim_channel,
                          (uint32_t *)g_dma_buf, (uint16_t)WS281X_DMA_BUF_WORDS);

    return 0;
}

/**
 * @brief  查询发送是否完成
 * @return 1: 正在发送, 0: 空闲
 */
static int stm32_timer_dma_is_busy(void)
{
    return g_stm32.busy ? 1 : 0;
}

/**
 * @brief  反初始化
 * @return 0: 成功
 */
static int stm32_timer_dma_deinit(void)
{
    if (g_stm32.busy)
    {
        HAL_TIM_PWM_Stop_DMA(g_stm32.htim, g_stm32.tim_channel);
    }

    HAL_TIM_PWM_DeInit(g_stm32.htim);

    if (g_stm32.dma_buf != NULL)
    {
        g_stm32.dma_buf = NULL;
    }

    memset(&g_stm32, 0, sizeof(g_stm32));
    return 0;
}

/**
 * @brief  DMA 半传输完成回调 (HT) — 上半区 A 刚发完
 * @note   HAL 弱函数重写，DMA 在循环模式下每次过半区触发一次
 */
void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim)
{
    if (htim != g_stm32.htim)
    {
        return;
    }

    fill_half(g_dma_buf, WS281X_DMA_HALF_WORDS);
    dcache_clean(g_dma_buf, WS281X_DMA_HALF_WORDS * sizeof(uint16_t));

    if (g_reset_cnt > 0U)
    {
        g_reset_cnt--;
        if (g_reset_cnt == 0U)
        {
            HAL_TIM_PWM_Stop_DMA(g_stm32.htim, g_stm32.tim_channel);
            g_stm32.busy = false;
        }
    }
    else if (g_src_remaining == 0U)
    {
        g_reset_cnt = 3U;
    }
}

/**
 * @brief  DMA 全传输完成回调 (TC) — 下半区 B 刚发完
 * @note   HAL 弱函数重写，DMA 循环模式下每次绕回起始处触发一次
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim != g_stm32.htim)
    {
        return;
    }

    fill_half(g_dma_buf + WS281X_DMA_HALF_WORDS, WS281X_DMA_HALF_WORDS);
    dcache_clean(g_dma_buf + WS281X_DMA_HALF_WORDS,
                 WS281X_DMA_HALF_WORDS * sizeof(uint16_t));

    if (g_reset_cnt > 0U)
    {
        g_reset_cnt--;
        if (g_reset_cnt == 0U)
        {
            HAL_TIM_PWM_Stop_DMA(g_stm32.htim, g_stm32.tim_channel);
            g_stm32.busy = false;
        }
    }
    else if (g_src_remaining == 0U)
    {
        g_reset_cnt = 3U;
    }
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  获取 STM32 Timer+DMA 驱动实例
 * @return ws281x_driver_t 结构体指针
 */
const ws281x_driver_t *ws281x_port_get_driver(void)
{
    static const ws281x_driver_t driver = {
        .init     = stm32_timer_dma_init,
        .transmit = stm32_timer_dma_transmit,
        .is_busy  = stm32_timer_dma_is_busy,
        .deinit   = stm32_timer_dma_deinit,
    };
    return &driver;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
