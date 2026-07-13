
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "bq40z80_port.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* 根据硬件原理图修改以下引脚和时钟配置 */
#ifndef BQ40Z80_I2C_PORT
#define BQ40Z80_I2C_PORT        I2C_NUM_0
#endif

#ifndef BQ40Z80_I2C_SDA_PIN
#define BQ40Z80_I2C_SDA_PIN     GPIO_NUM_47
#endif

#ifndef BQ40Z80_I2C_SCL_PIN
#define BQ40Z80_I2C_SCL_PIN     GPIO_NUM_33
#endif

#ifndef BQ40Z80_I2C_CLK_HZ
#define BQ40Z80_I2C_CLK_HZ      10000
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int esp32_i2c_init(void);
static int esp32_i2c_deinit(void);
static int esp32_i2c_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int esp32_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int esp32_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);
static int ensure_device(uint8_t dev_addr);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const char *TAG = "BQ40Z80_PORT";

static i2c_master_bus_handle_t g_bus_handle = NULL;
static i2c_master_dev_handle_t g_dev_handle = NULL;
static uint8_t                 g_cached_addr = 0;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 BQ40Z80 使用的 I2C 传输接口实例
 * @note   ESP32 平台使用硬件 I2C master driver（new API）。
 *         引脚和时钟频率通过上面的宏配置，也可在编译时通过 -D 选项覆盖。
 */
const i2c_transport_t *bq40z80_port_get_i2c(void)
{
    static const i2c_transport_t g_esp32_transport = {
        .init       = esp32_i2c_init,
        .deinit     = esp32_i2c_deinit,
        .write      = esp32_i2c_write,
        .read       = esp32_i2c_read,
        .write_read = esp32_i2c_write_read,
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
static int esp32_i2c_init(void)
{
    if (g_bus_handle != NULL)
    {
        return 0;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port                 = BQ40Z80_I2C_PORT,
        .sda_io_num               = BQ40Z80_I2C_SDA_PIN,
        .scl_io_num               = BQ40Z80_I2C_SCL_PIN,
        .clk_source               = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt        = 7,
        .intr_priority            = 0,
        .trans_queue_depth        = 0,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &g_bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %d", ret);
        return -2;
    }

    ESP_LOGW(TAG, "I2C init ok, SDA=%d SCL=%d clk=%dHz",
             BQ40Z80_I2C_SDA_PIN, BQ40Z80_I2C_SCL_PIN, BQ40Z80_I2C_CLK_HZ);
    return 0;
}

/**
 * @brief  反初始化 ESP32 硬件 I2C master 总线
 * @return 0: 成功
 */
static int esp32_i2c_deinit(void)
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
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 */
static int esp32_i2c_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
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
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 */
static int esp32_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms)
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
 * @return 0: 成功, -1: 参数错误, -3: NACK/超时
 */
static int esp32_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
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
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = BQ40Z80_I2C_CLK_HZ,
        .scl_wait_us     = 0,
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
