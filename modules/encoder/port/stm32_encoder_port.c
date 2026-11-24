/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "encoder_port.h"
#include "encoder_config.h"

/* 按实际 STM32 系列修改 HAL 头文件 */
#include "stm32f4xx_hal.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/* CubeMX 生成的定时器句柄；按实际实例修改，同时修改下面的 extern */
#ifndef ENCODER_STM32_TIM_HANDLE
#define ENCODER_STM32_TIM_HANDLE    &htim2
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static int      stm32_encoder_init(void);
static int32_t  stm32_encoder_read_count(void);
static int      stm32_encoder_deinit(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/* CubeMX 生成的定时器句柄；如果 tim.h 已经声明，可删除这一行 */
extern TIM_HandleTypeDef htim2;

static volatile int32_t g_encoder_rev; /* 整圈累计数，更新中断维护 */

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief 获取编码器 port 驱动实例
 *
 * @return 驱动实例指针
 */
const encoder_port_driver_t *encoder_port_get_driver(void)
{
    static const encoder_port_driver_t driver = {
        .init = stm32_encoder_init,
        .read_count = stm32_encoder_read_count,
        .deinit = stm32_encoder_deinit,
    };

    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief 初始化 STM32 定时器编码器接口
 *
 * @return 0: 成功, -1: 失败
 * @note 调用前需完成 CubeMX 的 MX_TIMx_Init()
 * @note CubeMX 中需把定时器配置为 Encoder Mode，PSC = 0；x4 时 CPR = PPR*4
 * @note 16 位定时器要求 CPR <= 65536
 * @note ARR 设为 counts_per_rev - 1，使计数器每转一圈产生一次更新事件
 */
static int stm32_encoder_init(void)
{
    TIM_HandleTypeDef *htim = ENCODER_STM32_TIM_HANDLE;
    uint32_t cpr = (uint32_t)ENCODER_COUNTS_PER_REV;

    if (htim == NULL || cpr == 0U)
    {
        return -1;
    }

    /* 一圈一次更新事件：计数器范围 0 ~ CPR-1 */
    __HAL_TIM_SET_AUTORELOAD(htim, cpr - 1U);
    __HAL_TIM_SET_COUNTER(htim, 0);

    /* 强制更新一次，让 ARR 预装载值立即生效 */
    __HAL_TIM_GENERATE_EVENT(htim, TIM_EVENTSOURCE_UPDATE);
    __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);
    g_encoder_rev = 0;

    if (HAL_TIM_Encoder_Start_IT(htim, TIM_CHANNEL_ALL) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief 读取累计计数
 *
 * @return 累计计数
 * @note 计数 = 整圈数 × CPR + 当前计数器值
 */
static int32_t stm32_encoder_read_count(void)
{
    TIM_HandleTypeDef *htim = ENCODER_STM32_TIM_HANDLE;
    int32_t rev;
    uint32_t raw;
    uint32_t primask;

    if (htim == NULL)
    {
        return 0;
    }

    /* 整圈数和计数器要成对读取，避免更新中断插入造成跳变 */
    primask = __get_PRIMASK();
    __disable_irq();
    rev = g_encoder_rev;
    raw = __HAL_TIM_GET_COUNTER(htim);

    /* 更新事件还没进中断时，按回绕方向补一圈 */
    if (__HAL_TIM_GET_FLAG(htim, TIM_FLAG_UPDATE) != RESET)
    {
        if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim))
        {
            if (raw > (ENCODER_COUNTS_PER_REV / 2U))
            {
                rev--;
            }
        }
        else
        {
            if (raw < (ENCODER_COUNTS_PER_REV / 2U))
            {
                rev++;
            }
        }
    }
    __set_PRIMASK(primask);

    return (int32_t)((int64_t)rev * (int64_t)ENCODER_COUNTS_PER_REV + (int64_t)raw);
}

/**
 * @brief 反初始化 STM32 定时器编码器接口
 *
 * @return 0: 成功
 */
static int stm32_encoder_deinit(void)
{
    TIM_HandleTypeDef *htim = ENCODER_STM32_TIM_HANDLE;

    if (htim != NULL)
    {
        HAL_TIM_Encoder_Stop_IT(htim, TIM_CHANNEL_ALL);
    }

    return 0;
}

/**
 * @brief TIM 更新事件回调，每圈一次
 *
 * @param htim 触发回调的定时器
 * @note 若工程里已有 HAL_TIM_PeriodElapsedCallback，请把这里的判断合并进去
 * @note 如需每圈业务逻辑，在此处追加
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == ENCODER_STM32_TIM_HANDLE)
    {
        if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim))
        {
            g_encoder_rev--;
        }
        else
        {
            g_encoder_rev++;
        }
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
