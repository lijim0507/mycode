/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "eeprom.h"
#include "swi2c.h"

#include <stdbool.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#define EEPROM_WRITE_CYCLE_MS  5

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int eeprom_write_page(uint16_t address, const uint8_t *data, uint16_t size);
static int eeprom_read_segment(uint16_t address, uint8_t *data, uint16_t size);
static int eeprom_send_write_header(uint16_t address);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const i2c_transport_t *g_transport;
static eeprom_config_t        g_config;
static bool                    g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 EEPROM 模块
 * @param  config    EEPROM 设备配置指针
 * @param  transport I2C 传输接口指针
 * @return 0: 成功, -1: 参数错误, -2: transport 无效
 */
int eeprom_init(eeprom_config_t *config, const i2c_transport_t *transport)
{
    if (!config || !config->delay_ms || config->page_size == 0
        || config->memory_size == 0
        || (config->addr_bytes != 1 && config->addr_bytes != 2))
    {
        return -1;
    }

    if (!transport
        || !transport->start || !transport->stop
        || !transport->send_byte || !transport->receive_byte
        || !transport->send_ack || !transport->receive_ack)
    {
        return -2;
    }

    g_config    = *config;
    g_transport = transport;
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

    g_transport   = NULL;
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
int eeprom_read_bytes(uint16_t address, uint8_t *data, uint16_t size)
{
    uint16_t remaining;
    uint16_t offset;
    uint16_t chunk;
    int      ret;

    if (!g_initialized)
    {
        return -2;
    }

    if (!data || size == 0 || (uint32_t)address + size > g_config.memory_size)
    {
        return -1;
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

        ret = eeprom_read_segment(address, &data[offset], chunk);
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
 * @return 0: 成功, -1: 参数错误, -2: 未初始化, -3: 从机 NACK
 */
int eeprom_write_bytes(uint16_t address, const uint8_t *data, uint16_t size)
{
    uint16_t remaining;
    uint16_t offset;
    uint16_t chunk;
    int      ret;

    if (!g_initialized)
    {
        return -2;
    }

    if (!data || size == 0 || (uint32_t)address + size > g_config.memory_size)
    {
        return -1;
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

        ret = eeprom_write_page(address, &data[offset], chunk);
        if (ret != 0)
        {
            if (g_config.wp_set)
            {
                g_config.wp_set(1);
            }
            return ret;
        }

        g_config.delay_ms(EEPROM_WRITE_CYCLE_MS);

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

/**
 * @brief  向 EEPROM 发送写地址头
 * @param  address 字地址
 * @return 0: 成功, -1: NACK
 */
static int eeprom_send_write_header(uint16_t address)
{
    g_transport->send_byte((uint8_t)((g_config.device_addr << 1) | 0x00));
    if (g_transport->receive_ack() != 0)
    {
        return -1;
    }

    if (g_config.addr_bytes == 2)
    {
        g_transport->send_byte((uint8_t)(address >> 8));
        if (g_transport->receive_ack() != 0)
        {
            return -1;
        }
    }
    g_transport->send_byte((uint8_t)(address & 0xFF));
    if (g_transport->receive_ack() != 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief  单页写入
 * @param  address  起始地址（需在同一页内）
 * @param  data     写入数据
 * @param  size     写入字节数（不超过当前页剩余空间）
 * @return 0: 成功, -3: 从机 NACK
 */
static int eeprom_write_page(uint16_t address, const uint8_t *data, uint16_t size)
{
    g_transport->start();

    if (eeprom_send_write_header(address) != 0)
    {
        g_transport->stop();
        return -3;
    }

    for (uint16_t i = 0; i < size; i++)
    {
        g_transport->send_byte(data[i]);
        if (g_transport->receive_ack() != 0)
        {
            g_transport->stop();
            return -3;
        }
    }

    g_transport->stop();
    return 0;
}

/**
 * @brief  单段读取（随机地址读取）
 * @param  address  起始地址
 * @param  data     读取数据缓冲区
 * @param  size     读取字节数
 * @return 0: 成功, -3: 从机 NACK
 */
static int eeprom_read_segment(uint16_t address, uint8_t *data, uint16_t size)
{
    g_transport->start();

    if (eeprom_send_write_header(address) != 0)
    {
        g_transport->stop();
        return -3;
    }

    g_transport->start();
    g_transport->send_byte((uint8_t)((g_config.device_addr << 1) | 0x01));
    if (g_transport->receive_ack() != 0)
    {
        g_transport->stop();
        return -3;
    }

    for (uint16_t i = 0; i < size; i++)
    {
        data[i] = g_transport->receive_byte();

        if (i < (uint16_t)(size - 1))
        {
            g_transport->send_ack(0);
        }
        else
        {
            g_transport->send_ack(1);
        }
    }

    g_transport->stop();
    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
