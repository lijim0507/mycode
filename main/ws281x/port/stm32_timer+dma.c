/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ws281x_port.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/*
 * ===================================================================
 *  STM32 Timer + DMA 移植层 (REFERENCE IMPLEMENTATION)
 *
 *  本文件为 STM32 平台的参考实现，使用定时器 PWM 输出 + DMA 方式
 *  驱动 WS281X 系列灯珠。当前项目目标平台为 ESP32-S3，本文件不
 *  参与编译，仅供移植参考。
 *
 *  原理：
 *    定时器以 PWM 模式产生固定周期 (1.25us) 的方波，高电平宽度
 *    区分逻辑 0/1：
 *      - 逻辑 0：高电平约 400ns
 *      - 逻辑 1：高电平约 800ns
 *    DMA 在每次定时器溢出 (Update 事件) 时自动将预计算的下一个
 *    CCR 值传输到比较寄存器，刷新占空比。CPU 仅在准备数据阶段
 *    参与，发送过程由硬件自动完成。
 *
 *  STM32 上使用时：
 *    1. 在项目编译选项中按需重定义下方各宏 (TIM 句柄/GPIO/LED 数)
 *    2. 替换 esp32_rmt.c 为本文件参与编译
 *    3. CubeMX 需预先配置：
 *       - 定时器通道 "PWM Generation CHx"
 *       - DMA MemToPeriph 关联该定时器 Update 请求
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
 * ---- 根据实际硬件修改以下宏定义 ----
 * 默认配置: TIM4_CH2, PD13, GPIO_AF2_TIM4, 240MHz, 10 灯
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
 * @brief  WS281X 时序参数 (固定值，无需修改)
 */
#define WS281X_BIT_PERIOD_NS      1250U
#define WS281X_T0H_NS              350U
#define WS281X_T1H_NS              850U

/****************************************************************************/
/*								Typedefs									*/
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
    uint32_t           buf_capacity;
    volatile bool      busy;
} stm32_timer_dma_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int stm32_timer_dma_init(void);
static int stm32_timer_dma_transmit(const uint8_t *data, uint32_t len);
static int stm32_timer_dma_is_busy(void);
static int stm32_timer_dma_deinit(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static stm32_timer_dma_ctx_t g_stm32;
#define WS281X_DMA_BUF_WORDS  (WS281X_MAX_FRAME_SIZE * 8 + 224)
static uint16_t g_dma_buf[WS281X_DMA_BUF_WORDS];

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  定时器 + DMA 硬件初始化
 * @return 0: 成功, -1: 时钟过低导致时序不可实现
 * @note   所有硬件配置通过文件顶部宏定义指定
 */
static int stm32_timer_dma_init(void)
{
    TIM_OC_InitTypeDef sConfigOC;
    GPIO_InitTypeDef gpio_init;
    TIM_HandleTypeDef *htim;
    uint32_t ticks_per_bit = 0;
    uint32_t ticks_t0h = 0;
    uint32_t ticks_t1h = 0;

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

    g_stm32.buf_capacity = WS281X_MAX_FRAME_SIZE * 8 + 224;
    g_stm32.dma_buf = g_dma_buf;

    htim = WS281X_STM32_TIM_HANDLE;

    htim->Init.Prescaler         = WS281X_STM32_TIM_CLK_PSC - 1U;
    htim->Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim->Init.Period            = g_stm32.arr;
    htim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(htim);

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
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
 * @return 0: 成功, -1: 上次传输未完成
 */
static int stm32_timer_dma_transmit(const uint8_t *data, uint32_t len)
{
    uint32_t bit_idx;

    if (g_stm32.busy)
    {
        return -1;
    }

    memset(g_stm32.dma_buf, 0, g_stm32.buf_capacity * sizeof(uint16_t));

    bit_idx = 0;

    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        for (int8_t b = 7; b >= 0; b--)
        {
            if (byte & (1U << (uint8_t)b))
            {
                g_stm32.dma_buf[bit_idx] = g_stm32.code_1;
            }
            else
            {
                g_stm32.dma_buf[bit_idx] = g_stm32.code_0;
            }
            bit_idx++;
        }
    }

    SCB_CleanDCache_by_Addr((uint32_t *)((uintptr_t)g_stm32.dma_buf & ~0x1FU),
                            (g_stm32.buf_capacity * sizeof(uint16_t) + 32U + ((uintptr_t)g_stm32.dma_buf & 0x1FU)));

    g_stm32.busy = true;

    HAL_TIM_PWM_Start_DMA(g_stm32.htim, g_stm32.tim_channel,
                          (uint32_t *)g_stm32.dma_buf, (uint16_t)g_stm32.buf_capacity);

    return 0;
}

/**
 * @brief  查询发送是否完成
 * @return 1: 正在发送, 0: 空闲
 */
static int stm32_timer_dma_is_busy(void)
{
    if (g_stm32.busy)
    {
        DMA_HandleTypeDef *hdma;

        hdma = g_stm32.htim->hdma[(uint8_t)(g_stm32.tim_channel >> 2U)];
        if (hdma != NULL && hdma->State == HAL_DMA_STATE_READY)
        {
            HAL_TIM_PWM_Stop_DMA(g_stm32.htim, g_stm32.tim_channel);
            g_stm32.busy = false;
        }
    }

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
 * @brief  DMA 发送完成回调 (HAL 弱函数重写)
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == g_stm32.htim)
    {
        HAL_TIM_PWM_Stop_DMA(g_stm32.htim, g_stm32.tim_channel);
        g_stm32.busy = false;
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