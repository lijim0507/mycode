
#ifndef __SWI2C_PORT_H_
#define __SWI2C_PORT_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "swi2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  SWI2C 平台配置（ESP32 / 通用 GPIO 位带）
 */
typedef struct {
    uint8_t  scl_pin;
    uint8_t  sda_pin;
    uint32_t delay_us;
} swi2c_port_cfg_t;

/****************************************************************************/
/*                      Exproted Variables                                  */
/****************************************************************************/

/****************************************************************************/
/*                      Exproted Functions                                  */
/****************************************************************************/

/**
 * @brief  获取当前平台的 SWI2C 硬件驱动实例
 * @return 驱动实例指针，调用失败返回 NULL
 * @note   根据编译时选中的 port/*.c 文件返回对应的驱动实现
 */
const swi2c_driver_t *swi2c_port_get_driver(void);

/**
 * @brief  获取 ESP32 硬件 I2C 的事务级传输接口实例
 * @return i2c_transport_t 结构体指针
 * @note   使用 ESP-IDF I2C master driver（i2c_master.h），
 *         引脚通过 ESP32_I2C_SDA_GPIO / ESP32_I2C_SCL_GPIO 宏配置。
 *         与 swi2c_get_transport() 接口相同，但底层是硬件 I2C 而非 GPIO 位带。
 */
const i2c_transport_t *esp32_i2c_port_get_transport(void);

/**
 * @brief  获取 STM32 硬件 I2C 的事务级传输接口实例
 * @return i2c_transport_t 结构体指针
 * @note   使用 STM32 HAL I2C（HAL_I2C_Master_Transmit / HAL_I2C_Mem_Read），
 *         I2C 句柄通过 STM32_I2C_PORT_HANDLE 宏配置（默认 hi2c1）。
 *         与 swi2c_get_transport() 接口相同，但底层是硬件 I2C 而非 GPIO 位带。
 */
const i2c_transport_t *stm32_i2c_port_get_transport(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
