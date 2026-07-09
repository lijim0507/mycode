/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "swi2c_port.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#ifndef ESP32_I2C_PORT_NUM
#define ESP32_I2C_PORT_NUM       I2C_NUM_0
#endif

#ifndef ESP32_I2C_SDA_GPIO
#define ESP32_I2C_SDA_GPIO       GPIO_NUM_21
#endif

#ifndef ESP32_I2C_SCL_GPIO
#define ESP32_I2C_SCL_GPIO       GPIO_NUM_22
#endif

#ifndef ESP32_I2C_FREQ_HZ
#define ESP32_I2C_FREQ_HZ        100000
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  esp32_i2c_init(void);
static int  esp32_i2c_deinit(void);
static int  esp32_i2c_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int  esp32_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int  esp32_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                  uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);
static int  esp32_i2c_get_dev_handle(uint8_t dev_addr, i2c_master_dev_handle_t *out_handle);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static i2c_master_bus_handle_t g_bus_handle;
static bool                    g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 ESP32 硬件 I2C 事务级传输接口实例
 * @note   首次调用时自动初始化 I2C 总线。
 *         SDA/SCL 引脚通过 ESP32_I2C_SDA_GPIO / ESP32_I2C_SCL_GPIO 宏配置。
 */
const i2c_transport_t *esp32_i2c_port_get_transport(void)
{
    static bool              transport_ready;
    static i2c_transport_t   transport;

    if (!transport_ready)
    {
        if (esp32_i2c_init() != 0)
        {
            return NULL;
        }

        transport.write      = esp32_i2c_write;
        transport.read       = esp32_i2c_read;
        transport.write_read = esp32_i2c_write_read;
        transport_ready      = true;
    }

    return &transport;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 ESP32 硬件 I2C 总线
 */
static int esp32_i2c_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port                 = ESP32_I2C_PORT_NUM,
        .sda_io_num               = ESP32_I2C_SDA_GPIO,
        .scl_io_num               = ESP32_I2C_SCL_GPIO,
        .clk_source               = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt        = 7,
        .flags.enable_internal_pullup = true,
    };

    if (i2c_new_master_bus(&bus_cfg, &g_bus_handle) != ESP_OK)
    {
        return -1;
    }

    g_initialized = true;
    return 0;
}

/**
 * @brief  反初始化 ESP32 硬件 I2C 总线
 */
static int esp32_i2c_deinit(void)
{
    if (g_initialized)
    {
        i2c_del_master_bus(g_bus_handle);
        g_bus_handle  = NULL;
        g_initialized = false;
    }

    return 0;
}

/**
 * @brief  为指定设备地址创建临时 device handle
 * @return 0: 成功, -1: 失败
 */
static int esp32_i2c_get_dev_handle(uint8_t dev_addr, i2c_master_dev_handle_t *out_handle)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = ESP32_I2C_FREQ_HZ,
    };

    if (i2c_master_bus_add_device(g_bus_handle, &dev_cfg, out_handle) != ESP_OK)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief  硬件 I2C write
 */
static int esp32_i2c_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    i2c_master_dev_handle_t dev_handle;
    esp_err_t               ret;

    if (esp32_i2c_get_dev_handle(dev_addr, &dev_handle) != 0)
    {
        return -3;
    }

    ret = i2c_master_transmit(dev_handle, data, len, (int)timeout_ms);
    i2c_master_bus_rm_device(dev_handle);

    return (ret == ESP_OK) ? 0 : -3;
}

/**
 * @brief  硬件 I2C read
 */
static int esp32_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    i2c_master_dev_handle_t dev_handle;
    esp_err_t               ret;

    if (esp32_i2c_get_dev_handle(dev_addr, &dev_handle) != 0)
    {
        return -3;
    }

    ret = i2c_master_receive(dev_handle, data, len, (int)timeout_ms);
    i2c_master_bus_rm_device(dev_handle);

    return (ret == ESP_OK) ? 0 : -3;
}

/**
 * @brief  硬件 I2C write + repeated start + read
 */
static int esp32_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                 uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms)
{
    i2c_master_dev_handle_t dev_handle;
    esp_err_t               ret;

    if (esp32_i2c_get_dev_handle(dev_addr, &dev_handle) != 0)
    {
        return -3;
    }

    ret = i2c_master_transmit_receive(dev_handle, wr_data, wr_len,
                                      rd_data, rd_len, (int)timeout_ms);
    i2c_master_bus_rm_device(dev_handle);

    return (ret == ESP_OK) ? 0 : -3;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
