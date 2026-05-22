
#ifndef __KEY_H_
#define __KEY_H_
/****************************************************************************/
/*							Includes									*/
/****************************************************************************/
#include <stdint.h>

/****************************************************************************/
/*							Macros										*/
/****************************************************************************/
/* 按键数量，请根据实际硬件修改 */
#define KEY_NUM             2

/* 按键硬件配置，请根据实际 GPIO 连接修改 */
#define KEY0_GPIO_PORT      GPIOE
#define KEY0_GPIO_PIN       GPIO_PIN_3
#define KEY0_ACTIVE_LEVEL   0       /* 按下有效电平：0 低电平按下 / 1 高电平按下 */

#define KEY1_GPIO_PORT      GPIOE
#define KEY1_GPIO_PIN       GPIO_PIN_4
#define KEY1_ACTIVE_LEVEL   0

/* GPIO 读取抽象接口，适配不同平台时修改此处 */
#define KEY0_GPIO_READ()    HAL_GPIO_ReadPin(KEY0_GPIO_PORT, KEY0_GPIO_PIN)
#define KEY1_GPIO_READ()    HAL_GPIO_ReadPin(KEY1_GPIO_PORT, KEY1_GPIO_PIN)

/****************************************************************************/
/*							Typedefs										*/
/****************************************************************************/
/**
 * @brief  按键索引枚举
 */
typedef enum _key_idx_t
{
    KEY_IDX_0 = 0,
    KEY_IDX_1,
}key_idx_t;

/**
 * @brief  按键事件枚举
 */
typedef enum _key_event_t
{
    KEY_EVENT_NONE = 0,     /* 无事件 */
    KEY_EVENT_PRESSED,      /* 按下事件 */
    KEY_EVENT_RELEASED,     /* 释放事件 */
}key_event_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/
void key_init(void);
void key_scan(void);
key_event_t key_get_event(uint8_t index);
uint8_t key_is_pressed(uint8_t index);

#endif
/****************************************************************************/
/*							EOF											*/
/****************************************************************************/
