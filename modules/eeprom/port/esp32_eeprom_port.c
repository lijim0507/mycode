/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "eeprom_port.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#ifndef EEPROM_ESP32_SPI_HOST
#define EEPROM_ESP32_SPI_HOST       SPI3_HOST
#endif

#ifndef EEPROM_ESP32_MOSI_GPIO
#define EEPROM_ESP32_MOSI_GPIO      GPIO_NUM_23
#endif

#ifndef EEPROM_ESP32_MISO_GPIO
#define EEPROM_ESP32_MISO_GPIO      GPIO_NUM_19
#endif

#ifndef EEPROM_ESP32_CLK_GPIO
#define EEPROM_ESP32_CLK_GPIO       GPIO_NUM_18
#endif

#ifndef EEPROM_ESP32_CS_GPIO
#define EEPROM_ESP32_CS_GPIO        GPIO_NUM_5
#endif

#ifndef EEPROM_ESP32_CLK_SPEED
#define EEPROM_ESP32_CLK_SPEED      10000000    /* 10 MHz */
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  esp32_eeprom_spi_init(void);
static int  esp32_eeprom_spi_deinit(void);
static void esp32_eeprom_spi_cs_set(uint8_t level);
static int  esp32_eeprom_spi_write(const uint8_t *data, uint16_t len);
static int  esp32_eeprom_spi_read(uint8_t *data, uint16_t len);
static int  esp32_eeprom_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void esp32_eeprom_spi_delay_us(uint32_t us);
static void esp32_eeprom_spi_delay_ms(uint32_t ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static spi_device_handle_t g_spi_handle;
static bool                g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 I2C 传输接口（委托给 SWI2C）
 *
 * @return i2c_transport_t 指针
 */
const i2c_transport_t *eeprom_port_get_i2c(void)
{
    return swi2c_get_transport();
}

/**
 * @brief  获取 SPI 驱动实例（符合 spi_port_driver_t 标准接口）
 *
 * @return 指向静态 const spi_port_driver_t 的指针
 */
const spi_port_driver_t *eeprom_port_get_spi(void)
{
    static const spi_port_driver_t driver = {
        .init           = esp32_eeprom_spi_init,
        .deinit         = esp32_eeprom_spi_deinit,
        .cs_set         = esp32_eeprom_spi_cs_set,
        .spi_write      = esp32_eeprom_spi_write,
        .spi_read       = esp32_eeprom_spi_read,
        .spi_transceive = esp32_eeprom_spi_transceive,
        .delay_us       = esp32_eeprom_spi_delay_us,
        .delay_ms       = esp32_eeprom_spi_delay_ms,
    };

    return &driver;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 ESP32 SPI 外设和 CS GPIO
 *
 * @return 0: 成功, -1: 失败
 */
static int esp32_eeprom_spi_init(void)
{
    esp_err_t ret;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = EEPROM_ESP32_MOSI_GPIO,
        .miso_io_num     = EEPROM_ESP32_MISO_GPIO,
        .sclk_io_num     = EEPROM_ESP32_CLK_GPIO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4092,
    };

    ret = spi_bus_initialize(EEPROM_ESP32_SPI_HOST, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK)
    {
        return -1;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = EEPROM_ESP32_CLK_SPEED,
        .mode           = 0,
        .spics_io_num   = -1,
        .queue_size     = 1,
    };

    ret = spi_bus_add_device(EEPROM_ESP32_SPI_HOST, &dev_cfg, &g_spi_handle);
    if (ret != ESP_OK)
    {
        spi_bus_free(EEPROM_ESP32_SPI_HOST);
        return -1;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EEPROM_ESP32_CS_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLED,
    };
    gpio_config(&io_conf);
    gpio_set_level(EEPROM_ESP32_CS_GPIO, 1);

    g_initialized = true;
    return 0;
}

/**
 * @brief  释放 ESP32 SPI 外设资源
 *
 * @return 0: 成功
 */
static int esp32_eeprom_spi_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    spi_bus_remove_device(g_spi_handle);
    spi_bus_free(EEPROM_ESP32_SPI_HOST);
    gpio_reset_pin(EEPROM_ESP32_CS_GPIO);
    g_initialized = false;
    return 0;
}

/**
 * @brief  控制 CS 引脚
 *
 * @param  level  0: 选中, 1: 释放
 */
static void esp32_eeprom_spi_cs_set(uint8_t level)
{
    gpio_set_level(EEPROM_ESP32_CS_GPIO, level);
}

/**
 * @brief  SPI 只写传输
 *
 * @param  data  发送数据
 * @param  len   字节数
 * @return 0: 成功, -1: 失败
 */
static int esp32_eeprom_spi_write(const uint8_t *data, uint16_t len)
{
    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    return (spi_device_polling_transmit(g_spi_handle, &trans) == ESP_OK) ? 0 : -1;
}

/**
 * @brief  SPI 只读传输
 *
 * @param  data  接收缓冲区
 * @param  len   字节数
 * @return 0: 成功, -1: 失败
 */
static int esp32_eeprom_spi_read(uint8_t *data, uint16_t len)
{
    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = NULL,
        .rx_buffer = data,
    };

    return (spi_device_polling_transmit(g_spi_handle, &trans) == ESP_OK) ? 0 : -1;
}

/**
 * @brief  SPI 全双工传输
 *
 * @param  tx   发送数据
 * @param  rx   接收缓冲区
 * @param  len  字节数
 * @return 0: 成功, -1: 失败
 */
static int esp32_eeprom_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    return (spi_device_polling_transmit(g_spi_handle, &trans) == ESP_OK) ? 0 : -1;
}

/**
 * @brief  微秒延时
 *
 * @param  us  微秒数
 */
static void esp32_eeprom_spi_delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

/**
 * @brief  毫秒延时
 *
 * @param  ms  毫秒数
 */
static void esp32_eeprom_spi_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
