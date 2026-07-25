/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "swi2c_port.h"

#include "stm32f4xx_hal.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* 硬件配置 — 通过编译选项覆盖，无需修改源文件 */
#ifndef SWI2C_SCL_PORT
#define SWI2C_SCL_PORT          GPIOB
#endif

#ifndef SWI2C_SCL_PIN
#define SWI2C_SCL_PIN           GPIO_PIN_6
#endif

#ifndef SWI2C_SDA_PORT
#define SWI2C_SDA_PORT          GPIOB
#endif

#ifndef SWI2C_SDA_PIN
#define SWI2C_SDA_PIN           GPIO_PIN_7
#endif

#ifndef SWI2C_DELAY_US
#define SWI2C_DELAY_US          5
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int     swi2c_port_init(void);
static int     swi2c_port_deinit(void);
static void    swi2c_port_sda_set(uint8_t level);
static void    swi2c_port_scl_set(uint8_t level);
static uint8_t swi2c_port_sda_get(void);
static void    swi2c_port_delay_us(void);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

const swi2c_driver_t *swi2c_port_get_driver(void)
{
    static const swi2c_driver_t driver = {
        .init     = swi2c_port_init,
        .deinit   = swi2c_port_deinit,
        .sda_set  = swi2c_port_sda_set,
        .scl_set  = swi2c_port_scl_set,
        .sda_get  = swi2c_port_sda_get,
        .delay_us = swi2c_port_delay_us,
    };
    return &driver;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  STM32 GPIO 初始化 — 配置 SCL/SDA 为开漏输出 + 内部上拉
 * @return 0: 成功
 */
static int swi2c_port_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    gpio_init.Pin = SWI2C_SCL_PIN;
    HAL_GPIO_Init(SWI2C_SCL_PORT, &gpio_init);

    gpio_init.Pin = SWI2C_SDA_PIN;
    HAL_GPIO_Init(SWI2C_SDA_PORT, &gpio_init);

    /* 空闲状态：总线置高（开漏释放 + 上拉 → 高电平） */
    HAL_GPIO_WritePin(SWI2C_SCL_PORT, SWI2C_SCL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SWI2C_SDA_PORT, SWI2C_SDA_PIN, GPIO_PIN_SET);

    return 0;
}

/**
 * @brief  反初始化 — 释放 GPIO 资源
 * @return 0: 成功
 */
static int swi2c_port_deinit(void)
{
    HAL_GPIO_DeInit(SWI2C_SCL_PORT, SWI2C_SCL_PIN);
    HAL_GPIO_DeInit(SWI2C_SDA_PORT, SWI2C_SDA_PIN);
    return 0;
}

/**
 * @brief  设置 SDA 电平
 * @param  level 0: 低电平, 非零: 高电平（开漏释放）
 */
static void swi2c_port_sda_set(uint8_t level)
{
    HAL_GPIO_WritePin(SWI2C_SDA_PORT, SWI2C_SDA_PIN,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  设置 SCL 电平
 */
static void swi2c_port_scl_set(uint8_t level)
{
    HAL_GPIO_WritePin(SWI2C_SCL_PORT, SWI2C_SCL_PIN,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  读取 SDA 电平
 * @return 0: 低电平, 1: 高电平
 */
static uint8_t swi2c_port_sda_get(void)
{
    return (HAL_GPIO_ReadPin(SWI2C_SDA_PORT, SWI2C_SDA_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

/**
 * @brief  微秒级延时（I2C 半周期）
 * @note   基于 NOP 忙等循环。SystemCoreClock / 4 粗略折算循环体开销。
 *         精确时序要求高时建议用 DWT 周期计数器替代。
 */
static void swi2c_port_delay_us(void)
{
    uint32_t count = SWI2C_DELAY_US * (SystemCoreClock / 4000000U);
    while (count--)
    {
        __NOP();
    }
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
