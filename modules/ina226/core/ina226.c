/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "ina226.h"

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* Configuration 寄存器 (00h) 位域 */
#define INA226_CFG_RST              (0x8000U)
#define INA226_CFG_AVG_MASK         (0x0E00U)
#define INA226_CFG_AVG_SHIFT        (9U)
#define INA226_CFG_VBUS_CT_MASK     (0x01C0U)
#define INA226_CFG_VBUS_CT_SHIFT    (6U)
#define INA226_CFG_VSH_CT_MASK      (0x0038U)
#define INA226_CFG_VSH_CT_SHIFT     (3U)
#define INA226_CFG_MODE_MASK        (0x0007U)
#define INA226_CFG_FIELDS_MASK      (INA226_CFG_AVG_MASK | INA226_CFG_VBUS_CT_MASK | \
                                     INA226_CFG_VSH_CT_MASK | INA226_CFG_MODE_MASK)

/* Mask/Enable 寄存器 (06h) 位域 */
#define INA226_MASK_AFF             (0x0010U)
#define INA226_MASK_CVRF            (0x0008U)
#define INA226_MASK_APOL            (0x0002U)
#define INA226_MASK_LEN             (0x0001U)

/* 标定系数：CAL = 0.00512 / (Current_LSB[A] × R_SHUNT[Ω])，
 * 换算为 Current_LSB[µA] 与 R_SHUNT[mΩ] 后为 5120000 / (LSB × R) */
#define INA226_CAL_FACTOR           (5120000U)

/* Calibration 寄存器 D15 保留，有效位 D14~D0 */
#define INA226_CAL_MAX              (0x7FFFU)

/* 物理量换算 */
/* 分流电压 2.5µV/LSB，用 raw × 5 / 2 换算以避免浮点 */
#define INA226_SHUNT_LSB_NUM        (5U)
#define INA226_SHUNT_LSB_DEN        (2U)
#define INA226_BUS_LSB_UV           (1250U)     /* 总线电压 1.25mV/LSB */
#define INA226_POWER_LSB_RATIO      (25U)       /* 功率 LSB = 25 × Current_LSB */

/* Alert Limit 寄存器可表示的最大电压阈值 */
#define INA226_ALERT_SHUNT_LIMIT_UV_MAX (163837U)     /* 65535 × 2.5µV */
#define INA226_ALERT_BUS_LIMIT_UV_MAX   (81918750U)   /* 65535 × 1.25mV */


/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int ina226_configure(void);
static int ina226_read_bits(ina226_reg_t reg, uint16_t mask, uint16_t shift, uint16_t *value);
static int ina226_write_bits(ina226_reg_t reg, uint16_t mask, uint16_t shift, uint16_t value);
static int32_t ina226_clamp_to_i32(int64_t value);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const i2c_transport_t *g_i2c;
static uint8_t                g_device_addr;
static uint16_t               g_shunt_mohm;
static uint32_t               g_current_lsb_ua;
static bool                   g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 INA226 模块
 * @return 0: 成功, -2: transport 无效或器件 ID 不匹配, 其他见返回值约定
 * @note   器件地址、分流电阻、最大电流、超时均取自 ina226_config.h 中的宏；
 *         内部完成器件 ID 校验、Configuration 默认值写入与 Calibration 计算。
 */
int ina226_init(void)
{
    int ret;

    g_i2c = ina226_port_get_i2c();
    if ((g_i2c == NULL) || (g_i2c->write == NULL) || (g_i2c->read == NULL) ||
        (g_i2c->write_read == NULL))
    {
        return -2;
    }

    if (g_i2c->init != NULL)
    {
        ret = g_i2c->init();
        if (ret != 0)
        {
            return ret;
        }
    }

    /* Current_LSB = Max_Expected_Current / 2^15，四舍五入到最近的 1µA */
    g_device_addr    = (uint8_t)INA226_DEFAULT_ADDR;
    g_shunt_mohm     = (uint16_t)INA226_DEFAULT_SHUNT_MOHM;
    g_current_lsb_ua = (uint32_t)(((uint64_t)INA226_DEFAULT_MAX_CURRENT_MA * 1000ULL +
                                  16384ULL) / 32768ULL);
    g_initialized    = true;

    ret = ina226_configure();
    if (ret != 0)
    {
        ina226_deinit();
        return ret;
    }

    return 0;
}

/**
 * @brief  反初始化 INA226 模块
 * @return 0: 成功
 */
int ina226_deinit(void)
{
    if ((g_i2c != NULL) && (g_i2c->deinit != NULL))
    {
        g_i2c->deinit();
    }

    g_i2c            = NULL;
    g_device_addr    = 0;
    g_shunt_mohm     = 0;
    g_current_lsb_ua = 0;
    g_initialized    = false;
    return 0;
}

/**
 * @brief  软件复位
 * @return 0: 成功, 其他见返回值约定
 * @note   只写入 RST 位，器件复位后配置与标定值回到默认值，
 *         需延时约 1ms 后重新调用 ina226_init()。
 */
int ina226_reset(void)
{
    uint16_t config;
    int      ret;

    if (!g_initialized)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_CONFIG, &config);
    if (ret != 0)
    {
        return ret;
    }

    config |= INA226_CFG_RST;
    return ina226_write_reg(INA226_REG_CONFIG, config);
}

/**
 * @brief  读取 16 位寄存器（MSB first）
 * @param  reg   寄存器地址
 * @param  value 输出寄存器原始值
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_read_reg(ina226_reg_t reg, uint16_t *value)
{
    uint8_t reg_addr;
    uint8_t rd_buf[2];
    int     ret;

    if (!g_initialized || (value == NULL))
    {
        return -1;
    }

    reg_addr = (uint8_t)reg;

    ret = g_i2c->write_read(g_device_addr, &reg_addr, 1, rd_buf, 2, INA226_DEFAULT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -3;
    }

    *value = (uint16_t)(((uint16_t)rd_buf[0] << 8) | (uint16_t)rd_buf[1]);
    return 0;
}

/**
 * @brief  写入 16 位寄存器（MSB first）
 * @param  reg   寄存器地址
 * @param  value 写入值
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_write_reg(ina226_reg_t reg, uint16_t value)
{
    uint8_t wr_buf[3];
    int     ret;

    if (!g_initialized)
    {
        return -1;
    }

    wr_buf[0] = (uint8_t)reg;
    wr_buf[1] = (uint8_t)(value >> 8);
    wr_buf[2] = (uint8_t)(value & 0xFFU);

    ret = g_i2c->write(g_device_addr, wr_buf, 3, INA226_DEFAULT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -3;
    }

    return 0;
}

/**
 * @brief  读取分流电压
 * @param  shunt_uv 输出分流电压，单位 µV
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_shunt_voltage_uv(int32_t *shunt_uv)
{
    uint16_t raw;
    int      ret;

    if (shunt_uv == NULL)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_SHUNT_VOLTAGE, &raw);
    if (ret != 0)
    {
        return ret;
    }

    /* 2.5µV/LSB，补码有符号 */
    *shunt_uv = (int32_t)((int16_t)raw) * (int32_t)INA226_SHUNT_LSB_NUM / (int32_t)INA226_SHUNT_LSB_DEN;
    return 0;
}

/**
 * @brief  读取总线电压
 * @param  bus_uv 输出总线电压，单位 µV
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_bus_voltage_uv(int32_t *bus_uv)
{
    uint16_t raw;
    int      ret;

    if (bus_uv == NULL)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_BUS_VOLTAGE, &raw);
    if (ret != 0)
    {
        return ret;
    }

    *bus_uv = (int32_t)raw * (int32_t)INA226_BUS_LSB_UV;
    return 0;
}

/**
 * @brief  读取电流
 * @param  current_ua 输出电流，单位 µA
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_current_ua(int32_t *current_ua)
{
    uint16_t raw;
    int      ret;

    if (current_ua == NULL)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_CURRENT, &raw);
    if (ret != 0)
    {
        return ret;
    }

    /* 补码有符号，LSB = Current_LSB */
    *current_ua = ina226_clamp_to_i32((int64_t)((int16_t)raw) * (int64_t)g_current_lsb_ua);
    return 0;
}

/**
 * @brief  读取功率
 * @param  power_uw 输出功率，单位 µW
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_power_uw(int32_t *power_uw)
{
    uint16_t raw;
    int      ret;

    if (power_uw == NULL)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_POWER, &raw);
    if (ret != 0)
    {
        return ret;
    }

    *power_uw = ina226_clamp_to_i32((int64_t)raw * (int64_t)INA226_POWER_LSB_RATIO * (int64_t)g_current_lsb_ua);
    return 0;
}

/**
 * @brief  读取 Calibration 寄存器
 * @param  cal 输出原始标定值
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_calibration(uint16_t *cal)
{
    if (cal == NULL)
    {
        return -1;
    }

    return ina226_read_reg(INA226_REG_CALIBRATION, cal);
}

/**
 * @brief  手动写入 Calibration 寄存器
 * @param  cal 标定值，有效范围 1 ~ 0x7FFF
 * @return 0: 成功, -1: 参数错误, -2: 标定值超出范围, 其他见返回值约定
 * @note   写入后 Current_LSB 按 0.00512 / (CAL × R_SHUNT) 重新换算。
 */
int ina226_set_calibration(uint16_t cal)
{
    int ret;

    if (!g_initialized || (cal == 0U) || (cal > INA226_CAL_MAX) || (g_shunt_mohm == 0U))
    {
        return -1;
    }

    ret = ina226_write_reg(INA226_REG_CALIBRATION, cal);
    if (ret != 0)
    {
        return ret;
    }

    g_current_lsb_ua = INA226_CAL_FACTOR / ((uint32_t)cal * (uint32_t)g_shunt_mohm);
    return 0;
}

/**
 * @brief  获取当前 Current_LSB
 * @param  lsb_ua 输出 Current_LSB，单位 µA
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_current_lsb_ua(uint32_t *lsb_ua)
{
    if (!g_initialized || (lsb_ua == NULL))
    {
        return -1;
    }

    *lsb_ua = g_current_lsb_ua;
    return 0;
}

/**
 * @brief  设置平均次数
 * @param  averaging 平均次数设置
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_averaging(ina226_averaging_t averaging)
{
    if (!g_initialized || ((uint32_t)averaging > (uint32_t)INA226_AVG_1024))
    {
        return -1;
    }

    return ina226_write_bits(INA226_REG_CONFIG, INA226_CFG_AVG_MASK, INA226_CFG_AVG_SHIFT,
                             (uint16_t)averaging);
}

/**
 * @brief  读取平均次数
 * @param  averaging 输出平均次数设置
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_averaging(ina226_averaging_t *averaging)
{
    uint16_t value;
    int      ret;

    if (averaging == NULL)
    {
        return -1;
    }

    ret = ina226_read_bits(INA226_REG_CONFIG, INA226_CFG_AVG_MASK, INA226_CFG_AVG_SHIFT, &value);
    if (ret != 0)
    {
        return ret;
    }

    *averaging = (ina226_averaging_t)value;
    return 0;
}

/**
 * @brief  设置分流/总线电压转换时间
 * @param  shunt_ct 分流电压转换时间
 * @param  bus_ct   总线电压转换时间
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_conv_time(ina226_conv_time_t shunt_ct, ina226_conv_time_t bus_ct)
{
    uint16_t config;
    int      ret;

    if (!g_initialized ||
        ((uint32_t)shunt_ct > (uint32_t)INA226_CT_8244US) ||
        ((uint32_t)bus_ct > (uint32_t)INA226_CT_8244US))
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_CONFIG, &config);
    if (ret != 0)
    {
        return ret;
    }

    config &= (uint16_t)(~(INA226_CFG_VSH_CT_MASK | INA226_CFG_VBUS_CT_MASK));
    config |= (uint16_t)(((uint16_t)shunt_ct << INA226_CFG_VSH_CT_SHIFT) & INA226_CFG_VSH_CT_MASK);
    config |= (uint16_t)(((uint16_t)bus_ct << INA226_CFG_VBUS_CT_SHIFT) & INA226_CFG_VBUS_CT_MASK);

    return ina226_write_reg(INA226_REG_CONFIG, config);
}

/**
 * @brief  读取分流/总线电压转换时间
 * @param  shunt_ct 输出分流电压转换时间
 * @param  bus_ct   输出总线电压转换时间
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_conv_time(ina226_conv_time_t *shunt_ct, ina226_conv_time_t *bus_ct)
{
    uint16_t config;
    int      ret;

    if ((shunt_ct == NULL) || (bus_ct == NULL))
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_CONFIG, &config);
    if (ret != 0)
    {
        return ret;
    }

    *shunt_ct = (ina226_conv_time_t)((config & INA226_CFG_VSH_CT_MASK) >> INA226_CFG_VSH_CT_SHIFT);
    *bus_ct   = (ina226_conv_time_t)((config & INA226_CFG_VBUS_CT_MASK) >> INA226_CFG_VBUS_CT_SHIFT);
    return 0;
}

/**
 * @brief  设置工作模式
 * @param  mode 工作模式
 * @return 0: 成功, 其他见返回值约定
 * @note   触发模式需重复写入同一模式才会重新启动一次转换。
 */
int ina226_set_mode(ina226_mode_t mode)
{
    if (!g_initialized || ((uint32_t)mode > (uint32_t)INA226_MODE_SHUNT_BUS_CONTINUOUS))
    {
        return -1;
    }

    return ina226_write_bits(INA226_REG_CONFIG, INA226_CFG_MODE_MASK, 0U, (uint16_t)mode);
}

/**
 * @brief  读取工作模式
 * @param  mode 输出工作模式
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_mode(ina226_mode_t *mode)
{
    uint16_t value;
    int      ret;

    if (mode == NULL)
    {
        return -1;
    }

    ret = ina226_read_bits(INA226_REG_CONFIG, INA226_CFG_MODE_MASK, 0U, &value);
    if (ret != 0)
    {
        return ret;
    }

    *mode = (ina226_mode_t)value;
    return 0;
}

/**
 * @brief  使能或关闭指定告警功能
 * @param  function 告警功能
 * @param  enable   true: 使能, false: 关闭
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_alert_function(ina226_alert_fn_t function, bool enable)
{
    uint16_t mask;

    if (!g_initialized || ((uint32_t)function > (uint32_t)INA226_ALERT_CONVERSION_READY))
    {
        return -1;
    }

    mask = (uint16_t)(1U << (uint16_t)function);
    return ina226_write_bits(INA226_REG_MASK_ENABLE, mask, 0U, enable ? mask : 0U);
}

/**
 * @brief  查询指定告警功能是否使能
 * @param  function 告警功能
 * @param  enable   输出使能状态
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_alert_function(ina226_alert_fn_t function, bool *enable)
{
    uint16_t value;
    uint16_t mask;
    int      ret;

    if ((uint32_t)function > (uint32_t)INA226_ALERT_CONVERSION_READY || (enable == NULL))
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_MASK_ENABLE, &value);
    if (ret != 0)
    {
        return ret;
    }

    mask    = (uint16_t)(1U << (uint16_t)function);
    *enable = ((value & mask) != 0U);
    return 0;
}

/**
 * @brief  设置告警锁存
 * @param  enable true: 锁存, false: 透明
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_alert_latch(bool enable)
{
    if (!g_initialized)
    {
        return -1;
    }

    return ina226_write_bits(INA226_REG_MASK_ENABLE, INA226_MASK_LEN, 0U,
                             enable ? INA226_MASK_LEN : 0U);
}

/**
 * @brief  查询告警锁存设置
 * @param  enable 输出锁存设置
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_alert_latch(bool *enable)
{
    uint16_t value;
    int      ret;

    if (enable == NULL)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_MASK_ENABLE, &value);
    if (ret != 0)
    {
        return ret;
    }

    *enable = ((value & INA226_MASK_LEN) != 0U);
    return 0;
}

/**
 * @brief  设置告警引脚极性
 * @param  polarity 告警引脚极性
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_alert_polarity(ina226_alert_polarity_t polarity)
{
    if (!g_initialized || ((uint32_t)polarity > (uint32_t)INA226_ALERT_POLARITY_ACTIVE_HIGH))
    {
        return -1;
    }

    return ina226_write_bits(INA226_REG_MASK_ENABLE, INA226_MASK_APOL, 0U,
                             (polarity == INA226_ALERT_POLARITY_ACTIVE_HIGH) ? INA226_MASK_APOL : 0U);
}

/**
 * @brief  查询告警引脚极性
 * @param  polarity 输出告警引脚极性
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_alert_polarity(ina226_alert_polarity_t *polarity)
{
    uint16_t value;
    int      ret;

    if (polarity == NULL)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_MASK_ENABLE, &value);
    if (ret != 0)
    {
        return ret;
    }

    *polarity = ((value & INA226_MASK_APOL) != 0U) ? INA226_ALERT_POLARITY_ACTIVE_HIGH
                                                   : INA226_ALERT_POLARITY_ACTIVE_LOW;
    return 0;
}

/**
 * @brief  写入 Alert Limit 寄存器原始值
 * @param  limit_raw 原始阈值
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_set_alert_limit_raw(uint16_t limit_raw)
{
    if (!g_initialized)
    {
        return -1;
    }

    return ina226_write_reg(INA226_REG_ALERT_LIMIT, limit_raw);
}

/**
 * @brief  读取 Alert Limit 寄存器原始值
 * @param  limit_raw 输出原始阈值
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_alert_limit_raw(uint16_t *limit_raw)
{
    if (limit_raw == NULL)
    {
        return -1;
    }

    return ina226_read_reg(INA226_REG_ALERT_LIMIT, limit_raw);
}

/**
 * @brief  设置分流电压告警阈值
 * @param  limit_uv 阈值，单位 µV，有效范围 0 ~ 163837
 * @return 0: 成功, -2: 阈值超出范围, 其他见返回值约定
 */
int ina226_set_shunt_alert_limit_uv(int32_t limit_uv)
{
    uint16_t raw;

    if (!g_initialized || (limit_uv < 0))
    {
        return -1;
    }

    if ((uint32_t)limit_uv > INA226_ALERT_SHUNT_LIMIT_UV_MAX)
    {
        return -2;
    }

    /* 分流电压 LSB = 2.5µV，四舍五入到最近的 LSB */
    raw = (uint16_t)(((uint32_t)limit_uv * 2U + 2U) / 5U);
    return ina226_write_reg(INA226_REG_ALERT_LIMIT, raw);
}

/**
 * @brief  设置总线电压告警阈值
 * @param  limit_uv 阈值，单位 µV，有效范围 0 ~ 81918750
 * @return 0: 成功, -2: 阈值超出范围, 其他见返回值约定
 */
int ina226_set_bus_alert_limit_uv(int32_t limit_uv)
{
    uint16_t raw;

    if (!g_initialized || (limit_uv < 0))
    {
        return -1;
    }

    if ((uint32_t)limit_uv > INA226_ALERT_BUS_LIMIT_UV_MAX)
    {
        return -2;
    }

    /* 总线电压 LSB = 1.25mV，四舍五入到最近的 LSB */
    raw = (uint16_t)(((uint32_t)limit_uv + (INA226_BUS_LSB_UV / 2U)) / INA226_BUS_LSB_UV);
    return ina226_write_reg(INA226_REG_ALERT_LIMIT, raw);
}

/**
 * @brief  读取 Mask/Enable 寄存器原始值
 * @param  value 输出原始值
 * @return 0: 成功, 其他见返回值约定
 * @note   读取该寄存器会清除 CVRF 与 AFF 标志位。
 */
int ina226_get_mask_enable(uint16_t *value)
{
    if (value == NULL)
    {
        return -1;
    }

    return ina226_read_reg(INA226_REG_MASK_ENABLE, value);
}

/**
 * @brief  查询告警功能标志（AFF）
 * @param  active 输出 true: 告警有效
 * @return 0: 成功, 其他见返回值约定
 * @note   读取 Mask/Enable 寄存器会清除 AFF，锁存模式下即解除锁存。
 */
int ina226_is_alert_active(bool *active)
{
    uint16_t value;
    int      ret;

    if (active == NULL)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_MASK_ENABLE, &value);
    if (ret != 0)
    {
        return ret;
    }

    *active = ((value & INA226_MASK_AFF) != 0U);
    return 0;
}

/**
 * @brief  查询转换完成标志（CVRF）
 * @param  ready 输出 true: 转换完成
 * @return 0: 成功, 其他见返回值约定
 * @note   读取 Mask/Enable 寄存器会清除 CVRF 标志。
 */
int ina226_is_conversion_ready(bool *ready)
{
    uint16_t value;
    int      ret;

    if (ready == NULL)
    {
        return -1;
    }

    ret = ina226_read_reg(INA226_REG_MASK_ENABLE, &value);
    if (ret != 0)
    {
        return ret;
    }

    *ready = ((value & INA226_MASK_CVRF) != 0U);
    return 0;
}

/**
 * @brief  轮询等待转换完成
 * @param  max_polls 最大轮询次数，传 0 使用 INA226_DEFAULT_READY_POLLS
 * @return 0: 已就绪, -4: 轮询次数耗尽仍未就绪, 其他见返回值约定
 * @note   每次轮询为一次 I2C 读操作，不依赖平台延时。
 */
int ina226_wait_conversion_ready(uint16_t max_polls)
{
    bool ready = false;
    int  ret;

    if (!g_initialized)
    {
        return -1;
    }

    if (max_polls == 0U)
    {
        max_polls = (uint16_t)INA226_DEFAULT_READY_POLLS;
    }

    for (uint16_t poll = 0U; poll < max_polls; poll++)
    {
        ret = ina226_is_conversion_ready(&ready);
        if (ret != 0)
        {
            return ret;
        }

        if (ready)
        {
            return 0;
        }
    }

    return -4;
}

/**
 * @brief  读取 Manufacturer ID
 * @param  id 输出厂商 ID
 * @return 0: 成功, 其他见返回值约定
 */
int ina226_get_manufacturer_id(uint16_t *id)
{
    if (id == NULL)
    {
        return -1;
    }

    return ina226_read_reg(INA226_REG_MANUFACTURER_ID, id);
}

/**
 * @brief  读取 Die ID
 * @param  die_id 输出 Die ID
 * @return 0: 成功, 其他见返回值约定 */
int ina226_get_die_id(uint16_t *die_id)
{
    if (die_id == NULL)
    {
        return -1;
    }

    return ina226_read_reg(INA226_REG_DIE_ID, die_id);
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  写入 Configuration 默认值与 Calibration
 * @return 0: 成功, -1: 配置宏非法, -2: 器件 ID 不匹配或标定值超出范围, -3: 总线通信失败
 * @note   Configuration 采用读-改-写，保留器件保留位（D14~D12）并保持 RST 为 0。
 */
static int ina226_configure(void)
{
    uint16_t manufacturer_id;
    uint16_t config;
    uint16_t cal;
    int      ret;

    if ((g_shunt_mohm == 0U) || (g_current_lsb_ua == 0U))
    {
        return -1;
    }

    ret = ina226_get_manufacturer_id(&manufacturer_id);
    if (ret != 0)
    {
        return ret;
    }

    if (manufacturer_id != INA226_MANUFACTURER_ID)
    {
        return -2;
    }

    ret = ina226_read_reg(INA226_REG_CONFIG, &config);
    if (ret != 0)
    {
        return ret;
    }

    config &= (uint16_t)(~INA226_CFG_FIELDS_MASK);
    config &= (uint16_t)(~INA226_CFG_RST);
    config |= (uint16_t)(((uint16_t)INA226_DEFAULT_AVERAGING << INA226_CFG_AVG_SHIFT) & INA226_CFG_AVG_MASK);
    config |= (uint16_t)(((uint16_t)INA226_DEFAULT_BUS_CONV_TIME << INA226_CFG_VBUS_CT_SHIFT) & INA226_CFG_VBUS_CT_MASK);
    config |= (uint16_t)(((uint16_t)INA226_DEFAULT_SHUNT_CONV_TIME << INA226_CFG_VSH_CT_SHIFT) & INA226_CFG_VSH_CT_MASK);
    config |= (uint16_t)((uint16_t)INA226_DEFAULT_MODE & INA226_CFG_MODE_MASK);

    ret = ina226_write_reg(INA226_REG_CONFIG, config);
    if (ret != 0)
    {
        return ret;
    }

    cal = (uint16_t)(INA226_CAL_FACTOR / ((uint32_t)g_current_lsb_ua * (uint32_t)g_shunt_mohm));
    if ((cal == 0U) || (cal > INA226_CAL_MAX))
    {
        return -2;
    }

    return ina226_write_reg(INA226_REG_CALIBRATION, cal);
}

/**
 * @brief  读取寄存器位域
 * @param  reg   寄存器地址
 * @param  mask  位域掩码
 * @param  shift 位域右移位数
 * @param  value 输出位域值
 * @return 0: 成功, 其他见返回值约定
 */
static int ina226_read_bits(ina226_reg_t reg, uint16_t mask, uint16_t shift, uint16_t *value)
{
    uint16_t reg_value;
    int      ret;

    ret = ina226_read_reg(reg, &reg_value);
    if (ret != 0)
    {
        return ret;
    }

    *value = (uint16_t)((reg_value & mask) >> shift);
    return 0;
}

/**
 * @brief  读-改-写寄存器位域
 * @param  reg   寄存器地址
 * @param  mask  位域掩码
 * @param  shift 位域左移位数
 * @param  value 待写入的位域值
 * @return 0: 成功, 其他见返回值约定
 */
static int ina226_write_bits(ina226_reg_t reg, uint16_t mask, uint16_t shift, uint16_t value)
{
    uint16_t reg_value;
    int      ret;

    ret = ina226_read_reg(reg, &reg_value);
    if (ret != 0)
    {
        return ret;
    }

    reg_value &= (uint16_t)(~mask);
    reg_value |= (uint16_t)((value << shift) & mask);

    return ina226_write_reg(reg, reg_value);
}

/**
 * @brief  将 64 位中间结果钳位到 int32_t 范围
 * @param  value 待钳位数值
 * @return 钳位后的 int32 值
 */
static int32_t ina226_clamp_to_i32(int64_t value)
{
    if (value > (int64_t)INT32_MAX)
    {
        return INT32_MAX;
    }

    if (value < (int64_t)INT32_MIN)
    {
        return INT32_MIN;
    }

    return (int32_t)value;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/