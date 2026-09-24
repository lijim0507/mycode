#ifndef __INA226_CONFIG_H_
#define __INA226_CONFIG_H_
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

/* --- 器件与硬件参数，可在编译选项中用 -D 覆盖 --- */

/* 7 位 I2C 器件地址，由 A0/A1 引脚决定，取值范围 0x40 ~ 0x4F */
#ifndef INA226_DEFAULT_ADDR
#define INA226_DEFAULT_ADDR             0x40
#endif

/* 分流电阻阻值，单位 mΩ */
#ifndef INA226_DEFAULT_SHUNT_MOHM
#define INA226_DEFAULT_SHUNT_MOHM       2
#endif

/* 最大预期电流，单位 mA，用于计算 Current_LSB = Max_Current / 2^15 */
#ifndef INA226_DEFAULT_MAX_CURRENT_MA
#define INA226_DEFAULT_MAX_CURRENT_MA   8000
#endif

/* 单次 I2C 事务超时，单位 ms */
#ifndef INA226_DEFAULT_TIMEOUT_MS
#define INA226_DEFAULT_TIMEOUT_MS       100
#endif

/* ina226_wait_conversion_ready() 传入 0 时使用的默认轮询次数 */
#ifndef INA226_DEFAULT_READY_POLLS
#define INA226_DEFAULT_READY_POLLS      32
#endif

/* --- 初始化时写入 Configuration 寄存器 (00h) 的默认设置 --- */
/* 下列默认值宏引用文件末尾 Typedefs 中的枚举，在 ina226.c 中展开 */

/* 平均次数 */
#ifndef INA226_DEFAULT_AVERAGING
#define INA226_DEFAULT_AVERAGING        INA226_AVG_4
#endif

/* 总线电压转换时间 */
#ifndef INA226_DEFAULT_BUS_CONV_TIME
#define INA226_DEFAULT_BUS_CONV_TIME    INA226_CT_1100US
#endif

/* 分流电压转换时间 */
#ifndef INA226_DEFAULT_SHUNT_CONV_TIME
#define INA226_DEFAULT_SHUNT_CONV_TIME  INA226_CT_1100US
#endif

/* 工作模式 */
#ifndef INA226_DEFAULT_MODE
#define INA226_DEFAULT_MODE             INA226_MODE_SHUNT_BUS_CONTINUOUS
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  平均次数设置（Configuration 寄存器 AVG[11:9]）
 */
typedef enum
{
    INA226_AVG_1    = 0,
    INA226_AVG_4    = 1,
    INA226_AVG_16   = 2,
    INA226_AVG_64   = 3,
    INA226_AVG_128  = 4,
    INA226_AVG_256  = 5,
    INA226_AVG_512  = 6,
    INA226_AVG_1024 = 7,
} ina226_averaging_t;

/**
 * @brief  电压转换时间设置（VBUSCT[8:6] / VSHCT[5:3]）
 */
typedef enum
{
    INA226_CT_140US  = 0,
    INA226_CT_204US  = 1,
    INA226_CT_332US  = 2,
    INA226_CT_588US  = 3,
    INA226_CT_1100US = 4,
    INA226_CT_2116US = 5,
    INA226_CT_4156US = 6,
    INA226_CT_8244US = 7,
} ina226_conv_time_t;

/**
 * @brief  工作模式设置（MODE[2:0]）
 */
typedef enum
{
    INA226_MODE_POWER_DOWN           = 0,
    INA226_MODE_SHUNT_TRIGGERED      = 1,
    INA226_MODE_BUS_TRIGGERED        = 2,
    INA226_MODE_SHUNT_BUS_TRIGGERED  = 3,
    INA226_MODE_SHUTDOWN             = 4,
    INA226_MODE_SHUNT_CONTINUOUS     = 5,
    INA226_MODE_BUS_CONTINUOUS       = 6,
    INA226_MODE_SHUNT_BUS_CONTINUOUS = 7,
} ina226_mode_t;

/**
 * @brief  告警功能选择（Mask/Enable 寄存器 D15~D10）
 * @note   枚举值即位号，可直接移位使用。
 */
typedef enum
{
    INA226_ALERT_SHUNT_OVER_VOLTAGE  = 15,
    INA226_ALERT_SHUNT_UNDER_VOLTAGE = 14,
    INA226_ALERT_BUS_OVER_VOLTAGE    = 13,
    INA226_ALERT_BUS_UNDER_VOLTAGE   = 12,
    INA226_ALERT_POWER_OVER_LIMIT    = 11,
    INA226_ALERT_CONVERSION_READY    = 10,
} ina226_alert_fn_t;

/**
 * @brief  告警引脚极性（Mask/Enable 寄存器 APOL 位）
 */
typedef enum
{
    INA226_ALERT_POLARITY_ACTIVE_LOW  = 0,
    INA226_ALERT_POLARITY_ACTIVE_HIGH = 1,
} ina226_alert_polarity_t;

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/