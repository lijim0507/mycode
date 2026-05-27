/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "eeprom.h"
#include "swi2c.h"

#include <stdbool.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#define EEPROM_WRITE_CYCLE_MS        5
#define EEPROM_SPI_WRITE_TIMEOUT_MS  10

#define EEPROM_SPI_WREN              0x06
#define EEPROM_SPI_WRITE             0x02
#define EEPROM_SPI_READ              0x03
#define EEPROM_SPI_RDSR              0x05
#define EEPROM_SPI_WIP_BIT           0x01

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

/* --- I2C 内部函数 --- */
static int eeprom_i2c_write_page(uint32_t address, const uint8_t *data, uint16_t size);
static int eeprom_i2c_read_chunk(uint32_t address, uint8_t *data, uint16_t size);
static int eeprom_i2c_send_write_header(uint32_t address);

/* --- SPI 内部函数 --- */
static int  eeprom_spi_write_page(uint32_t address, const uint8_t *data, uint16_t size);
static int  eeprom_spi_read_chunk(uint32_t address, uint8_t *data, uint16_t size);
static void eeprom_spi_send_address(uint32_t address);
static int  eeprom_spi_wait_ready(void);

/* --- 通用内部函数 --- */
static int eeprom_validate_read_params(uint32_t address, const uint8_t *data, uint16_t size);
static int eeprom_validate_write_params(uint32_t address, const uint8_t *data, uint16_t size);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const i2c_transport_t       *g_i2c;
static const spi_eeprom_transport_t *g_spi;
static eeprom_config_t              g_config;
static bool                         g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 EEPROM 模块
 * @param  config    EEPROM 设备配置
 * @param  transport 传输接口（类型由 config->type 决定）
 * @return 0: 成功, -1: 参数错误, -2: transport 无效
 */
int eeprom_init(eeprom_config_t *config, const void *transport)
{
    if (!config || !config->delay_ms || config->page_size == 0
        || config->memory_size == 0)
    {
        return -1;
    }

    if (!transport)
    {
        return -2;
    }

    switch (config->type)
    {
    case EEPROM_TYPE_I2C:
    {
        if (config->addr_bytes < 1 || config->addr_bytes > 2)
        {
            return -1;
        }

        const i2c_transport_t *i2c = (const i2c_transport_t *)transport;
        if (!i2c->start || !i2c->stop
            || !i2c->send_byte || !i2c->receive_byte
            || !i2c->send_ack || !i2c->receive_ack)
        {
            return -2;
        }

        g_i2c  = i2c;
        g_spi  = NULL;
        break;
    }
    case EEPROM_TYPE_SPI:
    {
        if (config->addr_bytes < 1 || config->addr_bytes > 3)
        {
            return -1;
        }

        const spi_eeprom_transport_t *spi = (const spi_eeprom_transport_t *)transport;
        if (!spi->cs_set || !spi->transfer)
        {
            return -2;
        }

        g_spi  = spi;
        g_i2c  = NULL;
        break;
    }
    default:
        return -1;
    }

    g_config      = *config;
    g_initialized = true;
    return 0;
}

/**
 * @brief  反初始化 EEPROM 模块
 * @return 0: 成功
 */
int eeprom_deinit(void)
{
    if (g_config.wp_set)
    {
        g_config.wp_set(1);
    }

    g_i2c         = NULL;
    g_spi         = NULL;
    g_initialized = false;
    return 0;
}

/**
 * @brief  从 EEPROM 读取数据
 * @param  address  起始地址
 * @param  data     读取数据缓冲区
 * @param  size     读取字节数
 * @return 0: 成功, -1: 参数错误, -2: 未初始化, -3: 从机 NACK
 */
int eeprom_read_bytes(uint32_t address, uint8_t *data, uint16_t size)
{
    uint16_t remaining;
    uint16_t offset;
    uint16_t chunk;
    int      ret;

    ret = eeprom_validate_read_params(address, data, size);
    if (ret != 0)
    {
        return ret;
    }

    if (g_config.wp_set)
    {
        g_config.wp_set(0);
    }

    remaining = size;
    offset    = 0;

    while (remaining > 0)
    {
        chunk = g_config.page_size - (uint16_t)(address % g_config.page_size);
        if (chunk > remaining)
        {
            chunk = remaining;
        }

        if (g_config.type == EEPROM_TYPE_I2C)
        {
            ret = eeprom_i2c_read_chunk(address, &data[offset], chunk);
        }
        else
        {
            ret = eeprom_spi_read_chunk(address, &data[offset], chunk);
        }

        if (ret != 0)
        {
            if (g_config.wp_set)
            {
                g_config.wp_set(1);
            }
            return ret;
        }

        address   += chunk;
        offset    += chunk;
        remaining -= chunk;
    }

    if (g_config.wp_set)
    {
        g_config.wp_set(1);
    }

    return 0;
}

/**
 * @brief  向 EEPROM 写入数据
 * @param  address  起始地址
 * @param  data     写入数据缓冲区
 * @param  size     写入字节数
 * @return 0: 成功, -1: 参数错误, -2: 未初始化, -3: NACK / 写超时
 */
int eeprom_write_bytes(uint32_t address, const uint8_t *data, uint16_t size)
{
    uint16_t remaining;
    uint16_t offset;
    uint16_t chunk;
    int      ret;

    ret = eeprom_validate_write_params(address, data, size);
    if (ret != 0)
    {
        return ret;
    }

    if (g_config.wp_set)
    {
        g_config.wp_set(0);
    }

    remaining = size;
    offset    = 0;

    while (remaining > 0)
    {
        chunk = g_config.page_size - (uint16_t)(address % g_config.page_size);
        if (chunk > remaining)
        {
            chunk = remaining;
        }

        if (g_config.type == EEPROM_TYPE_I2C)
        {
            ret = eeprom_i2c_write_page(address, &data[offset], chunk);
        }
        else
        {
            ret = eeprom_spi_write_page(address, &data[offset], chunk);
        }

        if (ret != 0)
        {
            if (g_config.wp_set)
            {
                g_config.wp_set(1);
            }
            return ret;
        }

        if (g_config.type == EEPROM_TYPE_I2C)
        {
            g_config.delay_ms(EEPROM_WRITE_CYCLE_MS);
        }

        address   += chunk;
        offset    += chunk;
        remaining -= chunk;
    }

    if (g_config.wp_set)
    {
        g_config.wp_set(1);
    }

    return 0;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/****************************************************************************/
/*                          I2C Internal                                   */
/****************************************************************************/

/**
 * @brief  I2C: 发送写地址头（设备地址 + 字地址）
 */
static int eeprom_i2c_send_write_header(uint32_t address)
{
    g_i2c->send_byte((uint8_t)((g_config.device_addr << 1) | 0x00));
    if (g_i2c->receive_ack() != 0)
    {
        return -1;
    }

    for (int i = g_config.addr_bytes - 1; i >= 0; i--)
    {
        g_i2c->send_byte((uint8_t)(address >> (uint8_t)(i * 8)));
        if (g_i2c->receive_ack() != 0)
        {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief  I2C: 单页写入
 */
static int eeprom_i2c_write_page(uint32_t address, const uint8_t *data, uint16_t size)
{
    g_i2c->start();

    if (eeprom_i2c_send_write_header(address) != 0)
    {
        g_i2c->stop();
        return -3;
    }

    for (uint16_t i = 0; i < size; i++)
    {
        g_i2c->send_byte(data[i]);
        if (g_i2c->receive_ack() != 0)
        {
            g_i2c->stop();
            return -3;
        }
    }

    g_i2c->stop();
    return 0;
}

/**
 * @brief  I2C: 单段读取（随机地址读取 + 当前地址读取）
 */
static int eeprom_i2c_read_chunk(uint32_t address, uint8_t *data, uint16_t size)
{
    g_i2c->start();

    if (eeprom_i2c_send_write_header(address) != 0)
    {
        g_i2c->stop();
        return -3;
    }

    g_i2c->start();
    g_i2c->send_byte((uint8_t)((g_config.device_addr << 1) | 0x01));
    if (g_i2c->receive_ack() != 0)
    {
        g_i2c->stop();
        return -3;
    }

    for (uint16_t i = 0; i < size; i++)
    {
        data[i] = g_i2c->receive_byte();

        if (i < (uint16_t)(size - 1))
        {
            g_i2c->send_ack(0);
        }
        else
        {
            g_i2c->send_ack(1);
        }
    }

    g_i2c->stop();
    return 0;
}

/****************************************************************************/
/*                          SPI Internal                                    */
/****************************************************************************/

/**
 * @brief  SPI: 发送存储地址（MSB 优先）
 */
static void eeprom_spi_send_address(uint32_t address)
{
    for (int i = g_config.addr_bytes - 1; i >= 0; i--)
    {
        g_spi->transfer((uint8_t)(address >> (uint8_t)(i * 8)));
    }
}

/**
 * @brief  SPI: 等待写周期完成（轮询状态寄存器 WIP 位）
 * @return 0: 完成, -3: 超时
 */
static int eeprom_spi_wait_ready(void)
{
    uint32_t elapsed = 0;

    do
    {
        g_spi->cs_set(0);
        g_spi->transfer(EEPROM_SPI_RDSR);
        uint8_t status = g_spi->transfer(0x00);
        g_spi->cs_set(1);

        if (!(status & EEPROM_SPI_WIP_BIT))
        {
            return 0;
        }

        g_config.delay_ms(1);
        elapsed++;
    }
    while (elapsed < EEPROM_SPI_WRITE_TIMEOUT_MS);

    return -3;
}

/**
 * @brief  SPI: 单页写入
 */
static int eeprom_spi_write_page(uint32_t address, const uint8_t *data, uint16_t size)
{
    g_spi->cs_set(0);
    g_spi->transfer(EEPROM_SPI_WREN);
    g_spi->cs_set(1);

    g_spi->cs_set(0);
    g_spi->transfer(EEPROM_SPI_WRITE);
    eeprom_spi_send_address(address);

    for (uint16_t i = 0; i < size; i++)
    {
        g_spi->transfer(data[i]);
    }
    g_spi->cs_set(1);

    return eeprom_spi_wait_ready();
}

/**
 * @brief  SPI: 单段读取
 */
static int eeprom_spi_read_chunk(uint32_t address, uint8_t *data, uint16_t size)
{
    g_spi->cs_set(0);
    g_spi->transfer(EEPROM_SPI_READ);
    eeprom_spi_send_address(address);

    for (uint16_t i = 0; i < size; i++)
    {
        data[i] = g_spi->transfer(0x00);
    }
    g_spi->cs_set(1);

    return 0;
}

/****************************************************************************/
/*                          Common Internal                                 */
/****************************************************************************/

/**
 * @brief  校验读参数
 */
static int eeprom_validate_read_params(uint32_t address, const uint8_t *data, uint16_t size)
{
    if (!g_initialized)
    {
        return -2;
    }

    if (!data || size == 0 || address + size > g_config.memory_size)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief  校验写参数
 */
static int eeprom_validate_write_params(uint32_t address, const uint8_t *data, uint16_t size)
{
    if (!g_initialized)
    {
        return -2;
    }

    if (!data || size == 0 || address + size > g_config.memory_size)
    {
        return -1;
    }

    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
