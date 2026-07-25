
#ifndef __BQ40Z80_H_
#define __BQ40Z80_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "bq40z80_port.h"
/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#ifndef BQ40Z80_DEFAULT_ADDR
#define BQ40Z80_DEFAULT_ADDR    0x0B
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/


/**
 * @brief  BQ40Z80 SBS / TI 扩展命令码
 * @note   命令码遵循 SBS 1.1 规范及 bq40z50/bq40z80 常用扩展。
 *         字符串类命令（如 ManufacturerName）需使用 bq40z80_read_block() 读取。
 */
typedef enum
{
    BQ40Z80_CMD_MANUFACTURER_ACCESS = 0x00,
    BQ40Z80_CMD_REMAINING_CAPACITY_ALARM = 0x01,
    BQ40Z80_CMD_REMAINING_TIME_ALARM = 0x02,
    BQ40Z80_CMD_BATTERY_MODE = 0x03,
    BQ40Z80_CMD_AT_RATE = 0x04,
    BQ40Z80_CMD_AT_RATE_TIME_TO_FULL = 0x05,
    BQ40Z80_CMD_AT_RATE_TIME_TO_EMPTY = 0x06,
    BQ40Z80_CMD_AT_RATE_OK = 0x07,
    BQ40Z80_CMD_TEMPERATURE = 0x08,
    BQ40Z80_CMD_VOLTAGE = 0x09,
    BQ40Z80_CMD_CURRENT = 0x0A,
    BQ40Z80_CMD_AVERAGE_CURRENT = 0x0B,
    BQ40Z80_CMD_MAX_ERROR = 0x0C,
    BQ40Z80_CMD_RELATIVE_STATE_OF_CHARGE = 0x0D,
    BQ40Z80_CMD_ABSOLUTE_STATE_OF_CHARGE = 0x0E,
    BQ40Z80_CMD_REMAINING_CAPACITY = 0x0F,
    BQ40Z80_CMD_FULL_CHARGE_CAPACITY = 0x10,
    BQ40Z80_CMD_RUN_TIME_TO_EMPTY = 0x11,
    BQ40Z80_CMD_AVERAGE_TIME_TO_EMPTY = 0x12,
    BQ40Z80_CMD_AVERAGE_TIME_TO_FULL = 0x13,
    BQ40Z80_CMD_CHARGING_CURRENT = 0x14,
    BQ40Z80_CMD_CHARGING_VOLTAGE = 0x15,
    BQ40Z80_CMD_BATTERY_STATUS = 0x16,
    BQ40Z80_CMD_CYCLE_COUNT = 0x17,
    BQ40Z80_CMD_DESIGN_CAPACITY = 0x18,
    BQ40Z80_CMD_DESIGN_VOLTAGE = 0x19,
    BQ40Z80_CMD_SPECIFICATION_INFO = 0x1A,
    BQ40Z80_CMD_MANUFACTURER_DATE = 0x1B,
    BQ40Z80_CMD_SERIAL_NUMBER = 0x1C,
    BQ40Z80_CMD_MANUFACTURER_NAME = 0x20,
    BQ40Z80_CMD_DEVICE_NAME = 0x21,
    BQ40Z80_CMD_DEVICE_CHEMISTRY = 0x22,
    BQ40Z80_CMD_MANUFACTURER_DATA = 0x23,
    BQ40Z80_CMD_AUTHENTICATE = 0x2F,
    BQ40Z80_CMD_CELL_VOLTAGE_4 = 0x3C,
    BQ40Z80_CMD_CELL_VOLTAGE_3 = 0x3D,
    BQ40Z80_CMD_CELL_VOLTAGE_2 = 0x3E,
    BQ40Z80_CMD_CELL_VOLTAGE_1 = 0x3F,
} bq40z80_cmd_t;

/**
 * @brief  BQ40Z80 模块配置
 */
typedef struct
{
    uint8_t device_addr;
} bq40z80_config_t;

/****************************************************************************/
/*                      Exported Variables                                  */
/****************************************************************************/

/****************************************************************************/
/*                      Exported Functions                                  */
/****************************************************************************/

/**
 * @brief  初始化 BQ40Z80 模块
 * @return 0: 成功, -1: 参数错误, -2: transport 不完整
 */
int bq40z80_init(void);

/**
 * @brief  反初始化 BQ40Z80 模块
 * @return 0: 成功
 */
int bq40z80_deinit(void);

/**
 * @brief  SMBus read word
 * @param  cmd   命令码
 * @param  value 输出 16-bit 数据（小端）
 * @return 0: 成功, -1: 参数错误/未初始化, -3: 总线通信失败
 */
int bq40z80_read_word(bq40z80_cmd_t cmd, uint16_t *value);

/**
 * @brief  SMBus write word
 * @param  cmd   命令码
 * @param  value 要写入的 16-bit 数据（小端）
 * @return 0: 成功, -1: 参数错误/未初始化, -3: 总线通信失败
 */
int bq40z80_write_word(bq40z80_cmd_t cmd, uint16_t value);

/**
 * @brief  SMBus read block
 * @param  cmd   命令码
 * @param  data  数据接收缓冲区
 * @param  len   输入为缓冲区大小，输出为实际读取字节数
 * @return 0: 成功, -1: 参数错误或缓冲区不足, -3: 总线通信失败
 */
int bq40z80_read_block(bq40z80_cmd_t cmd, uint8_t *data, uint8_t *len);

/**
 * @brief  SMBus write block
 * @param  cmd   命令码
 * @param  data  要写入的数据
 * @param  len   数据长度
 * @return 0: 成功, -1: 参数错误/未初始化, -3: 总线通信失败
 */
int bq40z80_write_block(bq40z80_cmd_t cmd, const uint8_t *data, uint8_t len);

/* --- 常用便捷读取函数 --- */

/**
 * @brief  读取电池总电压
 * @param  voltage_mv 输出电压，单位 mV
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_voltage(uint16_t *voltage_mv);

/**
 * @brief  读取瞬时电流
 * @param  current_ma 输出电流，单位 mA（正值充电，负值放电）
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_current(int16_t *current_ma);

/**
 * @brief  读取平均电流
 * @param  current_ma 输出平均电流，单位 mA
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_average_current(int16_t *current_ma);

/**
 * @brief  读取电池温度
 * @param  temp_0_1k 输出温度，单位 0.1K
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_temperature(uint16_t *temp_0_1k);

/**
 * @brief  读取相对荷电状态
 * @param  rsoc 输出百分比 0~100
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_relative_state_of_charge(uint8_t *rsoc);

/**
 * @brief  读取绝对荷电状态
 * @param  asoc 输出百分比 0~100
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_absolute_state_of_charge(uint8_t *asoc);

/**
 * @brief  读取剩余容量
 * @param  capacity_mah 输出容量，单位 mAh
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_remaining_capacity(uint16_t *capacity_mah);

/**
 * @brief  读取满充容量
 * @param  capacity_mah 输出容量，单位 mAh
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_full_charge_capacity(uint16_t *capacity_mah);

/**
 * @brief  读取电池状态字
 * @param  status 输出 16-bit 状态字
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_battery_status(uint16_t *status);

/**
 * @brief  读取循环次数
 * @param  count 输出循环次数
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_cycle_count(uint16_t *count);

/**
 * @brief  读取设计容量
 * @param  capacity_mah 输出容量，单位 mAh
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_design_capacity(uint16_t *capacity_mah);

/**
 * @brief  读取设计电压
 * @param  voltage_mv 输出电压，单位 mV
 * @return 0: 成功, 其他见 bq40z80_read_word()
 */
int bq40z80_get_design_voltage(uint16_t *voltage_mv);

/**
 * @brief  读取单节电芯电压
 * @param  cell_index 电芯序号 0~3，对应 CellVoltage1~4
 * @param  voltage_mv 输出电压，单位 mV
 * @return 0: 成功, -1: 序号错误, -3: 总线通信失败
 */
int bq40z80_get_cell_voltage(uint8_t cell_index, uint16_t *voltage_mv);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
