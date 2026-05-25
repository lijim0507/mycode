
#ifndef __SWI2C_H_
#define __SWI2C_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

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
 * @brief  SWI2C 硬件驱动抽象接口（port 层）
 * @note   上层 core 层不关心具体硬件实现，只通过此接口调用 port 层。
 *         每个平台只需实现这六个函数指针即可完成移植。
 *         使用开漏输出 + 上拉电阻实现线与逻辑，与标准 I2C 硬件兼容。
 */
typedef struct swi2c_driver {
    int     (*init)(void *config);
    int     (*deinit)(void);
    void    (*sda_set)(uint8_t level);
    void    (*scl_set)(uint8_t level);
    uint8_t (*sda_get)(void);
    void    (*delay_us)(void);
} swi2c_driver_t;

/**
 * @brief  I2C 传输抽象接口（core 层对上层暴露）
 * @note   上层模块（如 EEPROM）通过此接口使用 I2C 总线，无需直接依赖 swi2c 全局函数。
 *         与 swi2c_driver_t 的区别：driver 面向硬件操作，transport 面向协议操作。
 */
typedef struct i2c_transport {
    int     (*start)(void);
    int     (*stop)(void);
    int     (*send_byte)(uint8_t byte);
    uint8_t (*receive_byte)(void);
    int     (*send_ack)(uint8_t bit);
    uint8_t (*receive_ack)(void);
} i2c_transport_t;

/****************************************************************************/
/*                      Exproted Variables                                  */
/****************************************************************************/

/****************************************************************************/
/*                      Exproted Functions                                  */
/****************************************************************************/

/**
 * @brief  初始化 SWI2C 驱动库
 * @param  driver   平台驱动指针
 * @param  port_cfg 平台相关配置（如 GPIO 引脚、时钟频率），传 NULL 使用默认值
 * @return 0: 成功, -1: 参数错误, -2: 硬件初始化失败
 */
int      swi2c_init(const swi2c_driver_t *driver, void *port_cfg);

/**
 * @brief  反初始化 SWI2C 驱动库
 * @return 0: 成功
 */
int      swi2c_deinit(void);

/**
 * @brief  发送 I2C 起始信号
 * @return 0: 成功, -1: 未初始化
 * @note   SDA 高->低，SCL 保持高
 */
int      swi2c_start(void);

/**
 * @brief  发送 I2C 停止信号
 * @return 0: 成功, -1: 未初始化
 * @note   SDA 低->高，SCL 保持高
 */
int      swi2c_stop(void);

/**
 * @brief  发送一个字节（MSB 优先）
 * @param  byte 要发送的字节
 * @return 0: 成功, -1: 未初始化
 */
int      swi2c_send_byte(uint8_t byte);

/**
 * @brief  接收一个字节（MSB 优先）
 * @return 接收到的字节
 */
uint8_t  swi2c_receive_byte(void);

/**
 * @brief  发送应答位（ACK/NACK）
 * @param  bit 0: ACK（通知从机继续发送）, 1: NACK（通知从机停止发送）
 * @return 0: 成功, -1: 未初始化
 */
int      swi2c_send_ack(uint8_t bit);

/**
 * @brief  接收应答位（ACK/NACK）
 * @return 0: ACK（从机应答）, 1: NACK（从机未应答）
 * @note   通常在发送一个字节后调用，检查从机是否正确接收
 */
uint8_t  swi2c_receive_ack(void);

/**
 * @brief  获取 I2C 传输接口实例（依赖注入）
 * @return i2c_transport_t 结构体指针，供上层模块（如 EEPROM）解耦调用
 * @note   调用方通过此接口访问 I2C 总线，无需直接引用 swi2c_xxx() 全局函数
 */
const i2c_transport_t *swi2c_get_transport(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
