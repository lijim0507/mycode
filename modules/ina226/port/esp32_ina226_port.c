/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "ina226_port.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* 根据硬件原理图修改以下 I2C 总线配置 */
#ifndef INA226_ESP32_I2C_PORT
#define INA226_ESP32_I2C_PORT       I2C_NUM_0
#endif

#ifndef INA226_ESP32_I2C_SDA_PIN
#define INA226_ESP32_I2C_SDA_PIN    GPIO_NUM_21
#endif

#ifndef INA226_ESP32_I2C_SCL_PIN
#define INA226_ESP32_I2C_SCL_PIN    GPIO_NUM_22
#endif

/* INA226 支持 fast mode，最高 400kHz */
#ifndef INA226_ESP32_I2C_CLK_HZ
#define INA226_ESP32_I2C_CLK_HZ     400000
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int esp32_ina226_init(void);
static int esp32_ina226_deinit(void);
static int esp32_ina226_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int esp32_ina226_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int esp32_ina226_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                   uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);
static int ensure_device(uint8_t dev_addr);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const char *TAG = "INA226_PORT";

static i2c_master_bus_handle_t g_bus_handle = NULL;
static i2c_master_dev_handle_t g_dev_handle = NULL;
static uint8_t                 g_cached_addr = 0;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 INA226 使用的 I2C 传输接口实例
 * @return 指向静态 const i2c_transport_t 的指针
 * @note   ESP32 平台使用硬件 I2C master driver（new API）。
 *         引脚和时钟频率通过上面的宏配置，也可在编译时通过 -D 选项覆盖。
 */
const i2c_transport_t *ina226_port_get_i2c(void)
{
    static const i2c_transport_t g_esp32_transport = {
        .init       = esp32_ina226_init,
        .deinit     = esp32_ina226_deinit,
        .write      = esp32_ina226_write,
        .read       = esp32_ina226_read,
        .write_read = esp32_ina226_write_read,
    };

    return &g_esp32_transport;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 ESP32 硬件 I2C master 总线
 * @return 0: 成功, -2: 硬件初始化失败
 */
static int esp32_ina226_init(void)
{
    if (g_bus_handle != NULL)
    {
        return 0;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port                     = INA226_ESP32_I2C_PORT,
        .sda_io_num                   = INA226_ESP32_I2C_SDA_PIN,
        .scl_io_num                   = INA226_ESP32_I2C_SCL_PIN,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = 7,
        .intr_priority                = 0,
        .trans_queue_depth            = 0,        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &g_bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %d", ret);
        return -2;
    }

    ESP_LOGW(TAG, "I2C init ok, SDA=%d SCL=%d clk=%dHz",
             INA226_ESP32_I2C_SDA_PIN, INA226_ESP32_I2C_SCL_PIN, INA226_ESP32_I2C_CLK_HZ);
    return 0;
}

/**
 * @brief  反初始化 ESP32 硬件 I2C master 总线
 * @return 0: 成功
 */
static int esp32_ina226_deinit(void)
{
    if (g_dev_handle != NULL)
    {
        i2c_master_bus_rm_device(g_dev_handle);
        g_dev_handle = NULL;
    }

    if (g_bus_handle != NULL)
    {
        i2c_del_master_bus(g_bus_handle);
        g_bus_handle = NULL;
    }

    g_cached_addr = 0;
    return 0;
}

/**
 * @brief  I2C 主机发送
 * @param  dev_addr   7-bit 设备地址
 * @param  data       数据缓冲区
 * @param  len        数据长度
 * @param  timeout_ms 超时时间（毫秒）
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 */
static int esp32_ina226_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    int ret = ensure_device(dev_addr);
    if (ret != 0)
    {
        return ret;
    }

    esp_err_t err = i2c_master_transmit(g_dev_handle, data, len, (int)timeout_ms);
    if (err == ESP_OK)
    {
        return 0;
    }
    return (err == ESP_ERR_INVALID_ARG) ? -1 : -3;
}

/**
 * @brief  I2C 主机接收
 * @param  dev_addr   7-bit 设备地址
 * @param  data       数据缓冲区
 * @param  len        数据长度
 * @param  timeout_ms 超时时间（毫秒）
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 */
static int esp32_ina226_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    int ret = ensure_device(dev_addr);
    if (ret != 0)
    {
        return ret;
    }

    esp_err_t err = i2c_master_receive(g_dev_handle, data, len, (int)timeout_ms);
    if (err == ESP_OK)
    {
        return 0;
    }
    return (err == ESP_ERR_INVALID_ARG) ? -1 : -3;
}

/**
 * @brief  I2C 先写后读（repeated start）
 * @param  dev_addr   7-bit 设备地址
 * @param  wr_data    待写入数据（寄存器地址）
 * @param  wr_len     写入长度
 * @param  rd_data    读取缓冲区
 * @param  rd_len     读取长度
 * @param  timeout_ms 超时时间（毫秒）
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 */
static int esp32_ina226_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                   uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms)
{
    int ret = ensure_device(dev_addr);
    if (ret != 0)
    {
        return ret;
    }

    esp_err_t err = i2c_master_transmit_receive(g_dev_handle, wr_data, wr_len,
                                                rd_data, rd_len, (int)timeout_ms);
    if (err == ESP_OK)
    {
        return 0;
    }
    return (err == ESP_ERR_INVALID_ARG) ? -1 : -3;
}

/**
 * @brief  确保 I2C 设备已添加到总线（按需切换设备地址）
 * @param  dev_addr 7-bit 设备地址
 * @return 0: 成功, -2: 添加设备失败
 */
static int ensure_device(uint8_t dev_addr)
{
    if (g_dev_handle != NULL && dev_addr == g_cached_addr)
    {
        return 0;
    }

    if (g_dev_handle != NULL)
    {
        i2c_master_bus_rm_device(g_dev_handle);
        g_dev_handle = NULL;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length         = I2C_ADDR_BIT_LEN_7,
        .device_address          = dev_addr,
        .scl_speed_hz            = INA226_ESP32_I2C_CLK_HZ,
        .scl_wait_us             = 0,
        .flags.disable_ack_check = false,
    };

    esp_err_t ret = i2c_master_bus_add_device(g_bus_handle, &dev_config, &g_dev_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "add device 0x%02X failed: %d", dev_addr, ret);
        return -2;
    }

    g_cached_addr = dev_addr;
    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/