/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "swi2c_port.h"

#include "stm32f4xx_hal.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#ifndef STM32_I2C_PORT_HANDLE
#define STM32_I2C_PORT_HANDLE    (&hi2c1)
#endif

#ifndef STM32_I2C_TIMEOUT_MS
#define STM32_I2C_TIMEOUT_MS     100
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int stm32_i2c_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int stm32_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int stm32_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                 uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 STM32 硬件 I2C 事务级传输接口实例
 * @note   使用 STM32 HAL I2C，句柄由 STM32_I2C_PORT_HANDLE 宏配置（默认 hi2c1）。
 *         调用前需确保 HAL_I2C_Init() 已完成（通常在 CubeMX 生成的初始化代码中）。
 */
const i2c_transport_t *stm32_i2c_port_get_transport(void)
{
    static const i2c_transport_t transport = {
        .write      = stm32_i2c_write,
        .read       = stm32_i2c_read,
        .write_read = stm32_i2c_write_read,
    };

    return &transport;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  硬件 I2C write（HAL 直接模式）
 */
static int stm32_i2c_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    uint32_t timeout = (timeout_ms != 0) ? timeout_ms : STM32_I2C_TIMEOUT_MS;

    if (HAL_I2C_Master_Transmit(STM32_I2C_PORT_HANDLE,
                                (uint16_t)(dev_addr << 1),
                                (uint8_t *)data, (uint16_t)len,
                                timeout) != HAL_OK)
    {
        return -3;
    }

    return 0;
}

/**
 * @brief  硬件 I2C read（HAL 直接模式）
 */
static int stm32_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    uint32_t timeout = (timeout_ms != 0) ? timeout_ms : STM32_I2C_TIMEOUT_MS;

    if (HAL_I2C_Master_Receive(STM32_I2C_PORT_HANDLE,
                               (uint16_t)(dev_addr << 1),
                               data, (uint16_t)len,
                               timeout) != HAL_OK)
    {
        return -3;
    }

    return 0;
}

/**
 * @brief  硬件 I2C write + repeated start + read（HAL Mem_Read 模式）
 * @note   使用 HAL_I2C_Mem_Read 实现，把 wr_data 当作内存地址发送。
 *         最大支持 2 字节 wr_data（对应 I2C_MEMADD_SIZE_16BIT）。
 *         超过 2 字节时返回 -1。
 */
static int stm32_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                 uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms)
{
    uint16_t mem_addr;
    uint16_t mem_size;
    uint32_t timeout;

    if (wr_len == 0 || wr_len > 2)
    {
        return -1;
    }

    timeout = (timeout_ms != 0) ? timeout_ms : STM32_I2C_TIMEOUT_MS;

    /* 将 wr_data 拼成大端内存地址 */
    mem_addr = wr_data[0];
    if (wr_len == 2)
    {
        mem_addr = (uint16_t)((mem_addr << 8) | wr_data[1]);
    }

    mem_size = (wr_len == 2) ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT;

    if (HAL_I2C_Mem_Read(STM32_I2C_PORT_HANDLE,
                         (uint16_t)(dev_addr << 1),
                         mem_addr, mem_size,
                         rd_data, (uint16_t)rd_len,
                         timeout) != HAL_OK)
    {
        return -3;
    }

    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
