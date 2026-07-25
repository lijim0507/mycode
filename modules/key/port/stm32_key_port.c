/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "key.h"

/*
 * STM32 示例——根据实际 HAL 取消注释对应头文件：
 * #include "stm32f4xx_hal.h"
 */

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* ---------- 根据实际硬件修改以下配置 ---------- */

#define KEY0_GPIO_PORT     GPIOE
#define KEY0_GPIO_PIN      GPIO_PIN_3
#define KEY0_ACTIVE_LEVEL  0

/* --------------------------------------------- */

/****************************************************************************/
/*                      Port Read / Tick Functions                          */
/****************************************************************************/

static uint8_t stm32_key_read(void)
{
    /* 归一化：按下 = 1，释放 = 0 */
    /*
    return (HAL_GPIO_ReadPin(KEY0_GPIO_PORT, KEY0_GPIO_PIN)
            == KEY0_ACTIVE_LEVEL) ? 1 : 0;
    */
    return 0;
}

static uint32_t stm32_key_tick(void)
{
    /* return HAL_GetTick(); */
    return 0;
}

/****************************************************************************/
/*                      Port Init Example                                   */
/****************************************************************************/

/**
 * @brief  初始化 STM32 按键 GPIO
 * @note   在 MX_GPIO_Init() 或应用初始化中配置即可，此处仅为示例骨架
 */
void stm32_key_port_init(void)
{
    /*
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Pin  = KEY0_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = KEY0_ACTIVE_LEVEL ? GPIO_PULLDOWN : GPIO_PULLUP;
    HAL_GPIO_Init(KEY0_GPIO_PORT, &GPIO_InitStruct);
    */
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
