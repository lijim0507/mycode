
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "bq40z80.h"

#include <stdbool.h>
#include <string.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#define BQ40Z80_TIMEOUT_MS          100

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const i2c_transport_t *g_i2c;
static bq40z80_config_t       g_config;
static bool                   g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 BQ40Z80 模块
 */
int bq40z80_init(const i2c_transport_t *i2c, const bq40z80_config_t *config)
{
    if (!i2c || !config)
    {
        return -1;
    }

    if (!i2c->write || !i2c->read || !i2c->write_read)
    {
        return -2;
    }

    g_config = *config;
    if (g_config.device_addr == 0)
    {
        g_config.device_addr = BQ40Z80_DEFAULT_ADDR;
    }

    g_i2c         = i2c;
    g_initialized = true;

    return 0;
}

/**
 * @brief  反初始化 BQ40Z80 模块
 */
int bq40z80_deinit(void)
{
    g_i2c         = NULL;
    g_initialized = false;
    memset(&g_config, 0, sizeof(g_config));
    return 0;
}

/**
 * @brief  SMBus read word
 */
int bq40z80_read_word(bq40z80_cmd_t cmd, uint16_t *value)
{
    uint8_t wr_buf[1];
    uint8_t rd_buf[2];
    int     ret;

    if (!g_initialized || !value)
    {
        return -1;
    }

    wr_buf[0] = (uint8_t)cmd;

    ret = g_i2c->write_read(g_config.device_addr,
                            wr_buf, 1,
                            rd_buf, 2,
                            BQ40Z80_TIMEOUT_MS);
    if (ret != 0)
    {
        return -3;
    }

    *value = (uint16_t)rd_buf[0] | ((uint16_t)rd_buf[1] << 8);
    return 0;
}

/**
 * @brief  SMBus write word
 */
int bq40z80_write_word(bq40z80_cmd_t cmd, uint16_t value)
{
    uint8_t wr_buf[3];
    int     ret;

    if (!g_initialized)
    {
        return -1;
    }

    wr_buf[0] = (uint8_t)cmd;
    wr_buf[1] = (uint8_t)(value & 0xFF);
    wr_buf[2] = (uint8_t)(value >> 8);

    ret = g_i2c->write(g_config.device_addr, wr_buf, 3, BQ40Z80_TIMEOUT_MS);
    if (ret != 0)
    {
        return -3;
    }

    return 0;
}

/**
 * @brief  SMBus read block
 * @note   分两步完成：先读计数字节，再发命令后读数据。
 *         中间有 STOP（非严格 SMBus 连续读取），bq40z80 对此容忍。
 *         对于 ManufacturerAccess block read，建议用 bq40z80_write_word() 写子命令，
 *         然后用 bq40z80_read_block() 读结果。
 */
int bq40z80_read_block(bq40z80_cmd_t cmd, uint8_t *data, uint8_t *len)
{
    uint8_t wr_buf[1];
    uint8_t count;
    uint8_t buf_len;
    int     ret;

    if (!g_initialized || !data || !len)
    {
        return -1;
    }

    buf_len = *len;
    if (buf_len == 0)
    {
        return -1;
    }

    wr_buf[0] = (uint8_t)cmd;

    /* 第一步：发命令，读计数字节 */
    ret = g_i2c->write_read(g_config.device_addr,
                            wr_buf, 1,
                            &count, 1,
                            BQ40Z80_TIMEOUT_MS);
    if (ret != 0)
    {
        return -3;
    }

    if (count == 0)
    {
        *len = 0;
        return 0;
    }

    if (count > buf_len)
    {
        *len = count;
        return -1;
    }

    /* 第二步：再发命令，读 count 字节数据 */
    ret = g_i2c->write_read(g_config.device_addr,
                            wr_buf, 1,
                            data, count,
                            BQ40Z80_TIMEOUT_MS);
    if (ret != 0)
    {
        *len = 0;
        return -3;
    }

    *len = count;
    return 0;
}

/**
 * @brief  SMBus write block
 */
int bq40z80_write_block(bq40z80_cmd_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t wr_buf[2 + 32]; /* cmd + count + data */
    int     ret;

    if (!g_initialized || !data || len == 0)
    {
        return -1;
    }

    wr_buf[0] = (uint8_t)cmd;
    wr_buf[1] = len;
    for (uint8_t i = 0; i < len; i++)
    {
        wr_buf[2 + i] = data[i];
    }

    ret = g_i2c->write(g_config.device_addr, wr_buf,
                       (uint16_t)(2 + len), BQ40Z80_TIMEOUT_MS);
    if (ret != 0)
    {
        return -3;
    }

    return 0;
}

/* --- 便捷读取函数 --- */

/**
 * @brief  读取电池总电压
 */
int bq40z80_get_voltage(uint16_t *voltage_mv)
{
    return bq40z80_read_word(BQ40Z80_CMD_VOLTAGE, voltage_mv);
}

/**
 * @brief  读取瞬时电流
 */
int bq40z80_get_current(int16_t *current_ma)
{
    return bq40z80_read_word(BQ40Z80_CMD_CURRENT, (uint16_t *)current_ma);
}

/**
 * @brief  读取平均电流
 */
int bq40z80_get_average_current(int16_t *current_ma)
{
    return bq40z80_read_word(BQ40Z80_CMD_AVERAGE_CURRENT, (uint16_t *)current_ma);
}

/**
 * @brief  读取电池温度
 */
int bq40z80_get_temperature(uint16_t *temp_0_1k)
{
    return bq40z80_read_word(BQ40Z80_CMD_TEMPERATURE, temp_0_1k);
}

/**
 * @brief  读取相对荷电状态
 */
int bq40z80_get_relative_state_of_charge(uint8_t *rsoc)
{
    uint16_t value;
    int      ret;

    ret = bq40z80_read_word(BQ40Z80_CMD_RELATIVE_STATE_OF_CHARGE, &value);
    if (ret != 0)
    {
        return ret;
    }

    *rsoc = (uint8_t)value;
    return 0;
}

/**
 * @brief  读取绝对荷电状态
 */
int bq40z80_get_absolute_state_of_charge(uint8_t *asoc)
{
    uint16_t value;
    int      ret;

    ret = bq40z80_read_word(BQ40Z80_CMD_ABSOLUTE_STATE_OF_CHARGE, &value);
    if (ret != 0)
    {
        return ret;
    }

    *asoc = (uint8_t)value;
    return 0;
}

/**
 * @brief  读取剩余容量
 */
int bq40z80_get_remaining_capacity(uint16_t *capacity_mah)
{
    return bq40z80_read_word(BQ40Z80_CMD_REMAINING_CAPACITY, capacity_mah);
}

/**
 * @brief  读取满充容量
 */
int bq40z80_get_full_charge_capacity(uint16_t *capacity_mah)
{
    return bq40z80_read_word(BQ40Z80_CMD_FULL_CHARGE_CAPACITY, capacity_mah);
}

/**
 * @brief  读取电池状态字
 */
int bq40z80_get_battery_status(uint16_t *status)
{
    return bq40z80_read_word(BQ40Z80_CMD_BATTERY_STATUS, status);
}

/**
 * @brief  读取循环次数
 */
int bq40z80_get_cycle_count(uint16_t *count)
{
    return bq40z80_read_word(BQ40Z80_CMD_CYCLE_COUNT, count);
}

/**
 * @brief  读取设计容量
 */
int bq40z80_get_design_capacity(uint16_t *capacity_mah)
{
    return bq40z80_read_word(BQ40Z80_CMD_DESIGN_CAPACITY, capacity_mah);
}

/**
 * @brief  读取设计电压
 */
int bq40z80_get_design_voltage(uint16_t *voltage_mv)
{
    return bq40z80_read_word(BQ40Z80_CMD_DESIGN_VOLTAGE, voltage_mv);
}

/**
 * @brief  读取单节电芯电压
 */
int bq40z80_get_cell_voltage(uint8_t cell_index, uint16_t *voltage_mv)
{
    bq40z80_cmd_t cmd;

    if (!voltage_mv)
    {
        return -1;
    }

    switch (cell_index)
    {
    case 0:
        cmd = BQ40Z80_CMD_CELL_VOLTAGE_1;
        break;
    case 1:
        cmd = BQ40Z80_CMD_CELL_VOLTAGE_2;
        break;
    case 2:
        cmd = BQ40Z80_CMD_CELL_VOLTAGE_3;
        break;
    case 3:
        cmd = BQ40Z80_CMD_CELL_VOLTAGE_4;
        break;
    default:
        return -1;
    }

    return bq40z80_read_word(cmd, voltage_mv);
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
