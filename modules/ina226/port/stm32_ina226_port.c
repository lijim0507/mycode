/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "ina226_port.h"

#include "stm32f4xx_hal.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* 根据硬件原理图修改以下配置 */
#ifndef INA226_STM32_I2C_HANDLE
#define INA226_STM32_I2C_HANDLE     &hi2c1
#endif

#ifndef INA226_STM32_I2C_TIMEOUT_MS
#define INA226_STM32_I2C_TIMEOUT_MS 100
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int stm32_ina226_init(void);
static int stm32_ina226_deinit(void);
static int stm32_ina226_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int stm32_ina226_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int stm32_ina226_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                   uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static I2C_HandleTypeDef *g_i2c_handle = NULL;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 INA226 使用的 I2C 传输接口实例
 * @return 指向静态 const i2c_transport_t 的指针
 * @note   STM32 平台使用 HAL I2C 硬件驱动。
 *         调用前需确保 HAL_Init() 和 MX_I2Cx_Init() 已完成。
 *         I2C 句柄默认使用 &hi2c1，可通过 -DINA226_STM32_I2C_HANDLE=&hi2c2 覆盖。
 */
const i2c_transport_t *ina226_port_get_i2c(void)
{
    static const i2c_transport_t g_stm32_transport = {
        .init       = stm32_ina226_init,
        .deinit     = stm32_ina226_deinit,
        .write      = stm32_ina226_write,
        .read       = stm32_ina226_read,
        .write_read = stm32_ina226_write_read,
    };

    return &g_stm32_transport;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 STM32 HAL I2C（仅记录句柄，硬件初始化由 CubeMX 生成的代码完成）
 * @return 0: 成功
 */
static int stm32_ina226_init(void)
{
    g_i2c_handle = INA226_STM32_I2C_HANDLE;
    return 0;
}

/**
 * @brief  反初始化 STM32 HAL I2C
 * @return 0: 成功
 */
static int stm32_ina226_deinit(void)
{
    if (g_i2c_handle != NULL)
    {
        HAL_I2C_DeInit(g_i2c_handle);
        g_i2c_handle = NULL;
    }
    return 0;
}

/**
 * @brief  I2C 主机发送
 * @param  dev_addr   7-bit 设备地址
 * @param  data       数据缓冲区
 * @param  len        数据长度
 * @param  timeout_ms 超时时间（毫秒），传 0 使用默认值
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 */
static int stm32_ina226_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (g_i2c_handle == NULL)
    {
        return -1;
    }

    if (timeout_ms == 0)
    {
        timeout_ms = INA226_STM32_I2C_TIMEOUT_MS;
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(g_i2c_handle,
                                                    (uint16_t)(dev_addr << 1),
                                                    (uint8_t *)data, len, timeout_ms);
    if (ret == HAL_OK)
    {
        return 0;
    }
    return (ret == HAL_ERROR) ? -3 : -1;
}

/**
 * @brief  I2C 主机接收
 * @param  dev_addr   7-bit 设备地址
 * @param  data       数据缓冲区
 * @param  len        数据长度
 * @param  timeout_ms 超时时间（毫秒），传 0 使用默认值
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 */
static int stm32_ina226_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (g_i2c_handle == NULL)
    {
        return -1;
    }

    if (timeout_ms == 0)
    {
        timeout_ms = INA226_STM32_I2C_TIMEOUT_MS;
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(g_i2c_handle,
                                                   (uint16_t)(dev_addr << 1),
                                                   data, len, timeout_ms);
    if (ret == HAL_OK)
    {
        return 0;
    }
    return (ret == HAL_ERROR) ? -3 : -1;
}

/**
 * @brief  I2C 先写后读（repeated start）
 * @param  dev_addr   7-bit 设备地址
 * @param  wr_data    待写入数据（寄存器地址）
 * @param  wr_len     写入长度
 * @param  rd_data    读取缓冲区
 * @param  rd_len     读取长度
 * @param  timeout_ms 超时时间（毫秒），传 0 使用默认值
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 * @note   STM32 HAL 的 Mem_Read 在发送完寄存器地址后自动插入 repeated start，
 *         与 INA226 读寄存器所需的时序一致。
 */
static int stm32_ina226_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                   uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms)
{
    if (g_i2c_handle == NULL)
    {
        return -1;
    }

    if (timeout_ms == 0)
    {
        timeout_ms = INA226_STM32_I2C_TIMEOUT_MS;
    }

    /* INA226 读寄存器只需单字节寄存器地址，走 Mem_Read */
    if (wr_len == 1)
    {
        HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(g_i2c_handle,
                                                 (uint16_t)(dev_addr << 1),
                                                 wr_data[0], I2C_MEMADD_SIZE_8BIT,
                                                 rd_data, rd_len, timeout_ms);
        if (ret == HAL_OK)
        {
            return 0;
        }
        return (ret == HAL_ERROR) ? -3 : -1;
    }

    /* 多字节写入 → 退化为 write + stop + read（无 repeated start） */
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(g_i2c_handle,
                                                    (uint16_t)(dev_addr << 1),
                                                    (uint8_t *)wr_data, wr_len, timeout_ms);
    if (ret != HAL_OK)
    {
        return (ret == HAL_ERROR) ? -3 : -1;
    }

    ret = HAL_I2C_Master_Receive(g_i2c_handle,
                                 (uint16_t)(dev_addr << 1),
                                 rd_data, rd_len, timeout_ms);
    if (ret == HAL_OK)
    {
        return 0;
    }
    return (ret == HAL_ERROR) ? -3 : -1;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/