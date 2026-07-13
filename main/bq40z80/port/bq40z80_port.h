
#ifndef __BQ40Z80_PORT_H_
#define __BQ40Z80_PORT_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>

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
 * @brief  I2C 传输抽象接口
 * @note   由各平台 port 文件实现，core 层通过此接口完成 SMBus 通信。
 *         各函数返回值约定：0=成功，-1=参数错误，-2=硬件错误，-3=总线 NACK/超时。
 */
typedef struct i2c_transport
{
    /**
     * @brief  初始化 I2C 硬件
     * @return 0: 成功, 负值: 错误码
     */
    int (*init)(void);

    /**
     * @brief  反初始化 I2C 硬件
     * @return 0: 成功
     */
    int (*deinit)(void);

    /**
     * @brief  主机发送
     * @param  dev_addr   7-bit 设备地址
     * @param  data       数据缓冲区
     * @param  len        数据长度
     * @param  timeout_ms 超时时间（毫秒），传 0 使用默认值
     * @return 0: 成功, -1: 参数错误, -3: NACK/超时
     */
    int (*write)(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);

    /**
     * @brief  主机接收
     * @param  dev_addr   7-bit 设备地址
     * @param  data       数据缓冲区
     * @param  len        数据长度
     * @param  timeout_ms 超时时间（毫秒），传 0 使用默认值
     * @return 0: 成功, -1: 参数错误, -3: NACK/超时
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
     * @return 0: 成功, -1: 参数错误, -3: NACK/超时
     * @note   对应 I2C 的 write + repeated start + read 时序，
     *         覆盖 SMBus read word/block 操作。
     */
    int (*write_read)(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                      uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);
} i2c_transport_t;

/****************************************************************************/
/*                      Exported Variables                                  */
/****************************************************************************/

/****************************************************************************/
/*                      Exported Functions                                  */
/****************************************************************************/

/**
 * @brief  获取 BQ40Z80 使用的 I2C 传输接口实例
 * @return i2c_transport_t 结构体指针
 * @note   由对应平台的 port 文件实现，负责 I2C 硬件的初始化和读写操作。
 *         上层调用者无需关心底层是硬件 I2C 还是软件 I2C。
 */
const i2c_transport_t *bq40z80_port_get_i2c(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
