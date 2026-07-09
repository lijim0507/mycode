
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
 * @brief  I2C 传输抽象接口（事务级）
 * @note   上层模块（如 EEPROM、BQ40Z80）通过此接口使用 I2C 总线。
 *         与 swi2c_driver_t 的区别：driver 面向硬件操作（位级），transport 面向事务操作。
 *         此接口与 ESP-IDF I2C master / STM32 HAL I2C 的 API 形状一致，
 *         方便各平台用硬件 I2C 直接实现。
 */
typedef struct i2c_transport {
    /**
     * @brief  主机发送
     * @param  dev_addr   7-bit 设备地址
     * @param  data       数据缓冲区
     * @param  len        数据长度
     * @param  timeout_ms 超时时间（毫秒），传 0 使用默认值
     * @return 0: 成功, 负值: 总线错误（-3: NACK/超时）
     */
    int (*write)(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);

    /**
     * @brief  主机接收
     * @param  dev_addr   7-bit 设备地址
     * @param  data       数据缓冲区
     * @param  len        数据长度
     * @param  timeout_ms 超时时间（毫秒），传 0 使用默认值
     * @return 0: 成功, 负值: 总线错误（-3: NACK/超时）
     */
    int (*read)(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);

    /**
     * @brief  先写后读（中间插入 repeated start）
     * @param  dev_addr   7-bit 设备地址
     * @param  wr_data    待写入数据
     * @param  wr_len     写入长度
     * @param  rd_data    读取缓冲区
     * @param  rd_len     读取长度
     * @param  timeout_ms 超时时间（毫秒），传 0 使用默认值
     * @return 0: 成功, 负值: 总线错误（-3: NACK/超时）
     * @note   对应 I2C 的 write + repeated start + read 时序，
     *         覆盖 SMBus read word/block、I2C EEPROM 随机读等常见操作。
     */
    int (*write_read)(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                      uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);
} i2c_transport_t;

#ifndef SWI2C_DEFAULT_TIMEOUT_MS
#define SWI2C_DEFAULT_TIMEOUT_MS    100
#endif

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
 * @brief  获取软件 I2C 的事务级传输接口实例
 * @return i2c_transport_t 结构体指针，供上层模块（如 EEPROM、BQ40Z80）解耦调用
 * @note   事务级 write/read/write_read 在内部由位级 GPIO 操作实现。
 *         硬件 I2C 端口请用 esp32_i2c_port_get_transport() / stm32_i2c_port_get_transport()。
 */
const i2c_transport_t *swi2c_get_transport(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
