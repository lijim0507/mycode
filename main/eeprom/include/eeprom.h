
#ifndef __EEPROM_H_
#define __EEPROM_H_
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

typedef struct i2c_transport i2c_transport_t;

/**
 * @brief  EEPROM 设备配置
 * @note   支持 24Cxx 系列及兼容 EEPROM（I2C 接口）
 */
typedef struct {
    uint8_t  device_addr;
    uint16_t page_size;
    uint32_t memory_size;
    uint8_t  addr_bytes;
    void     (*wp_set)(uint8_t level);
    void     (*delay_ms)(uint32_t ms);
} eeprom_config_t;

/****************************************************************************/
/*                      Exproted Variables                                  */
/****************************************************************************/

/****************************************************************************/
/*                      Exproted Functions                                  */
/****************************************************************************/

/**
 * @brief  初始化 EEPROM 模块
 * @param  config    EEPROM 设备配置指针
 * @param  transport I2C 传输接口指针（由 swi2c_get_transport() 获取）
 * @return 0: 成功, -1: 参数错误, -2: transport 无效
 */
int  eeprom_init(eeprom_config_t *config, const i2c_transport_t *transport);

/**
 * @brief  反初始化 EEPROM 模块
 * @return 0: 成功
 */
int  eeprom_deinit(void);

/**
 * @brief  从 EEPROM 读取数据（随机地址读取）
 * @param  address  起始地址
 * @param  data     读取数据缓冲区
 * @param  size     读取字节数
 * @return 0: 成功, -1: 参数错误, -2: 未初始化, -3: 从机 NACK
 * @note   自动处理页边界，支持跨页读取
 */
int  eeprom_read_bytes(uint16_t address, uint8_t *data, uint16_t size);

/**
 * @brief  向 EEPROM 写入数据（页写入）
 * @param  address  起始地址
 * @param  data     写入数据缓冲区
 * @param  size     写入字节数
 * @return 0: 成功, -1: 参数错误, -2: 未初始化, -3: 从机 NACK
 * @note   自动处理页边界，每页写入后等待 EEPROM 写周期（5ms）
 */
int  eeprom_write_bytes(uint16_t address, const uint8_t *data, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
