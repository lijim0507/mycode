#ifndef __INA226_H_
#define __INA226_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "ina226_config.h"
#include "ina226_port.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* Manufacturer ID 寄存器 (FEh) 固定值，对应 ASCII "TI" */
#define INA226_MANUFACTURER_ID      (0x5449U)

/* Die ID 寄存器 (FFh)：高 12 位为器件 ID，低 4 位为版本号 */
#define INA226_DIE_ID_MASK          (0xFFF0U)
#define INA226_DIE_ID_VALUE         (0x2260U)

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  INA226 寄存器地址
 */
typedef enum
{
    INA226_REG_CONFIG          = 0x00,
    INA226_REG_SHUNT_VOLTAGE   = 0x01,
    INA226_REG_BUS_VOLTAGE     = 0x02,
    INA226_REG_POWER           = 0x03,
    INA226_REG_CURRENT         = 0x04,
    INA226_REG_CALIBRATION     = 0x05,
    INA226_REG_MASK_ENABLE     = 0x06,
    INA226_REG_ALERT_LIMIT     = 0x07,
    INA226_REG_MANUFACTURER_ID = 0xFE,
    INA226_REG_DIE_ID          = 0xFF,
} ina226_reg_t;

/****************************************************************************/
/*                      Exported Variables                                  */
/****************************************************************************/

/****************************************************************************/
/*                      Exported Functions                                  */
/****************************************************************************/

/*
 * 返回值约定：
 *    0: 成功
 *   -1: 参数错误或模块未初始化
 *   -2: 硬件/器件错误（transport 无效、器件 ID 不匹配、标定值超出范围）
 *   -3: 总线通信失败（NACK / 超时）
 *   -4: 轮询超时（仅 ina226_wait_conversion_ready()）
 */

/* --- 初始化 --- */

/**
 * @brief  初始化 INA226 模块
 * @return 0: 成功, -2: transport 无效或器件 ID 不匹配, 其他见返回值约定
 * @note   内部调用 ina226_port_get_i2c() 获取 I2C 传输接口，
 *         器件地址、分流电阻、最大电流等参数由 ina226_config.h 中的宏提供。
 *         初始化完成后写入 Configuration 默认值并计算 Calibration 寄存器。
 */
int ina226_init(void);

/**
 * @brief  反初始化 INA226 模块
 * @return 0: 成功
 */
int ina226_deinit(void);

/**
 * @brief  软件复位，寄存器恢复上电默认值
 * @return 0: 成功, 其他见返回值约定
 * @note   仅写入 Configuration 寄存器的 RST 位，器件需约 1ms 完成复位，
 *         复位后配置与标定值丢失，需延时后重新调用 ina226_init()。
 */
int ina226_reset(void);

/* --- 原始寄存器访问 --- */

/**
 * @brief  读取 16 位寄存器（MSB first）
 * @param  reg   寄存器地址
 * @param  value 输出寄存器原始值
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_read_reg(ina226_reg_t reg, uint16_t *value);

/**
 * @brief  写入 16 位寄存器（MSB first）
 * @param  reg   寄存器地址
 * @param  value 写入值
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_write_reg(ina226_reg_t reg, uint16_t value);

/* --- 测量 --- */

/**
 * @brief  读取分流电压
 * @param  shunt_uv 输出分流电压，单位 µV（2.5µV/LSB）
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_shunt_voltage_uv(int32_t *shunt_uv);

/**
 * @brief  读取总线电压
 * @param  bus_uv 输出总线电压，单位 µV（1.25mV/LSB）
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_bus_voltage_uv(int32_t *bus_uv);

/**
 * @brief  读取电流
 * @param  current_ua 输出电流，单位 µA（LSB 由 Calibration 决定，正值流向负载）
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_current_ua(int32_t *current_ua);

/**
 * @brief  读取功率
 * @param  power_uw 输出功率，单位 µW（LSB = 25 × Current_LSB）
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_power_uw(int32_t *power_uw);

/* --- 标定 --- */

/**
 * @brief  读取 Calibration 寄存器
 * @param  cal 输出原始标定值（有效位 D14~D0）
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_calibration(uint16_t *cal);

/**
 * @brief  手动写入 Calibration 寄存器
 * @param  cal 标定值，有效范围 1 ~ 0x7FFF
 * @return 0: 成功, -1: 参数错误, -2: 标定值超出范围, 其他见返回值约定
 * @note   用于系统级校准（如按实测电流修正满量程），写入后
 *         Current_LSB 按 0.00512 / (CAL × R_SHUNT) 重新换算。
 */
int ina226_set_calibration(uint16_t cal);

/**
 * @brief  获取当前 Current_LSB
 * @param  lsb_ua 输出 Current_LSB，单位 µA
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_current_lsb_ua(uint32_t *lsb_ua);

/* --- 配置寄存器 (00h) --- */

/**
 * @brief  设置平均次数
 * @param  averaging 平均次数设置
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_averaging(ina226_averaging_t averaging);

/**
 * @brief  读取平均次数
 * @param  averaging 输出平均次数设置
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_averaging(ina226_averaging_t *averaging);

/**
 * @brief  设置分流/总线电压转换时间
 * @param  shunt_ct 分流电压转换时间
 * @param  bus_ct   总线电压转换时间
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_conv_time(ina226_conv_time_t shunt_ct, ina226_conv_time_t bus_ct);

/**
 * @brief  读取分流/总线电压转换时间
 * @param  shunt_ct 输出分流电压转换时间
 * @param  bus_ct   输出总线电压转换时间
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_conv_time(ina226_conv_time_t *shunt_ct, ina226_conv_time_t *bus_ct);

/**
 * @brief  设置工作模式
 * @param  mode 工作模式
 * @return 0: 成功, 其他见返回值约定
 * @note   触发模式（TRIGGERED）需重复写入同一模式才会重新启动一次转换。
 */
int ina226_set_mode(ina226_mode_t mode);

/**
 * @brief  读取工作模式
 * @param  mode 输出工作模式
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_mode(ina226_mode_t *mode);

/* --- 告警 (Mask/Enable + Alert Limit) --- */

/**
 * @brief  使能或关闭指定告警功能
 * @param  function 告警功能
 * @param  enable   true: 使能, false: 关闭
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_alert_function(ina226_alert_fn_t function, bool enable);

/**
 * @brief  查询指定告警功能是否使能
 * @param  function 告警功能
 * @param  enable   输出使能状态
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_alert_function(ina226_alert_fn_t function, bool *enable);

/**
 * @brief  设置告警锁存
 * @param  enable true: 锁存, false: 透明（告警源消失即复位）
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_alert_latch(bool enable);

/**
 * @brief  查询告警锁存设置
 * @param  enable 输出锁存设置
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_alert_latch(bool *enable);

/**
 * @brief  设置告警引脚极性
 * @param  polarity 告警引脚极性
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_alert_polarity(ina226_alert_polarity_t polarity);

/**
 * @brief  查询告警引脚极性
 * @param  polarity 输出告警引脚极性
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_alert_polarity(ina226_alert_polarity_t *polarity);

/**
 * @brief  写入 Alert Limit 寄存器原始值
 * @param  limit_raw 原始阈值，比较对象由使能的告警功能决定
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_alert_limit_raw(uint16_t limit_raw);

/**
 * @brief  读取 Alert Limit 寄存器原始值
 * @param  limit_raw 输出原始阈值
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_alert_limit_raw(uint16_t *limit_raw);

/**
 * @brief  设置分流电压过压/欠压告警阈值
 * @param  limit_uv 阈值，单位 µV，有效范围 0 ~ 163837
 * @return 0: 成功, -2: 阈值超出范围, 其他见返回值约定
 */
int ina226_set_shunt_alert_limit_uv(int32_t limit_uv);

/**
 * @brief  设置总线电压过压/欠压告警阈值
 * @param  limit_uv 阈值，单位 µV，有效范围 0 ~ 81918750
 * @return 0: 成功, -2: 阈值超出范围, 其他见返回值约定
 */
int ina226_set_bus_alert_limit_uv(int32_t limit_uv);

/**
 * @brief  读取 Mask/Enable 寄存器原始值
 * @param  value 输出原始值
 * @return 0: 成功, 其他见返回值约定
 * @note   读取该寄存器会清除 CVRF 与 AFF 标志位（含锁存的告警）。
 */
int ina226_get_mask_enable(uint16_t *value);

/**
 * @brief  查询告警功能标志（AFF）
 * @param  active 输出 true: 告警有效
 * @return 0: 成功, 其他见返回值约定
 * @note   读取 Mask/Enable 寄存器会清除 AFF 标志，锁存模式下也用于解除锁存。
 */
int ina226_is_alert_active(bool *active);

/* --- 状态与识别 --- */

/**
 * @brief  查询转换完成标志（CVRF）
 * @param  ready 输出 true: 转换完成
 * @return 0: 成功, 其他见返回值约定
 * @note   读取 Mask/Enable 寄存器会清除 CVRF 标志。
 */
int ina226_is_conversion_ready(bool *ready);

/**
 * @brief  轮询等待转换完成
 * @param  max_polls 最大轮询次数，传 0 使用 INA226_DEFAULT_READY_POLLS
 * @return 0: 已就绪, -4: 轮询次数耗尽仍未就绪, 其他见返回值约定
 * @note   每次轮询为一次 I2C 读操作，内部不依赖平台延时，
 *         阻塞时长由轮询次数决定。
 */
int ina226_wait_conversion_ready(uint16_t max_polls);

/**
 * @brief  读取 Manufacturer ID
 * @param  id 输出厂商 ID，正常应为 INA226_MANUFACTURER_ID
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_manufacturer_id(uint16_t *id);

/**
 * @brief  读取 Die ID
 * @param  die_id 输出 Die ID，(die_id & INA226_DIE_ID_MASK) 应为 INA226_DIE_ID_VALUE
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_die_id(uint16_t *die_id);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/