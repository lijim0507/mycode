/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "w25q80_port.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#ifndef W25Q80_ESP32_SPI_HOST
#define W25Q80_ESP32_SPI_HOST       SPI3_HOST
#endif

#ifndef W25Q80_ESP32_MOSI_GPIO
#define W25Q80_ESP32_MOSI_GPIO      GPIO_NUM_23
#endif

#ifndef W25Q80_ESP32_MISO_GPIO
#define W25Q80_ESP32_MISO_GPIO      GPIO_NUM_19
#endif

#ifndef W25Q80_ESP32_CLK_GPIO
#define W25Q80_ESP32_CLK_GPIO       GPIO_NUM_18
#endif

#ifndef W25Q80_ESP32_CS_GPIO
#define W25Q80_ESP32_CS_GPIO        GPIO_NUM_5
#endif

#ifndef W25Q80_ESP32_CLK_SPEED
#define W25Q80_ESP32_CLK_SPEED      40000000
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  esp32_w25q80_init(void);
static int  esp32_w25q80_deinit(void);
static void esp32_w25q80_cs_set(uint8_t level);
static int  esp32_w25q80_spi_write(const uint8_t *data, uint16_t len);
static int  esp32_w25q80_spi_read(uint8_t *data, uint16_t len);
static int  esp32_w25q80_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void esp32_w25q80_delay_us(uint32_t us);
static void esp32_w25q80_delay_ms(uint32_t ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static spi_device_handle_t g_spi_handle;
static bool                g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 ESP32 W25Q80 port driver 实例
 *
 * @return 指向静态 driver 结构体的指针
 */
const w25q80_port_driver_t *w25q80_port_get_driver(void)
{
    static const w25q80_port_driver_t driver = {
        .init           = esp32_w25q80_init,
        .deinit         = esp32_w25q80_deinit,
        .cs_set         = esp32_w25q80_cs_set,
        .spi_write      = esp32_w25q80_spi_write,
        .spi_read       = esp32_w25q80_spi_read,
        .spi_transceive = esp32_w25q80_spi_transceive,
        .delay_us       = esp32_w25q80_delay_us,
        .delay_ms       = esp32_w25q80_delay_ms,
    };

    return &driver;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 ESP32 SPI 外设和 CS GPIO
 *
 * @return 0: 成功, -1: ESP-IDF SPI 初始化失败
 */
static int esp32_w25q80_init(void)
{
    esp_err_t ret;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = W25Q80_ESP32_MOSI_GPIO,
        .miso_io_num     = W25Q80_ESP32_MISO_GPIO,
        .sclk_io_num     = W25Q80_ESP32_CLK_GPIO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4092,
    };

    ret = spi_bus_initialize(W25Q80_ESP32_SPI_HOST, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK)
    {
        return -1;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = W25Q80_ESP32_CLK_SPEED,
        .mode           = 0,                        /* CPOL=0, CPHA=0 */
        .spics_io_num   = -1,                       /* 软件控制 CS */
        .queue_size     = 1,
    };

    ret = spi_bus_add_device(W25Q80_ESP32_SPI_HOST, &dev_cfg, &g_spi_handle);
    if (ret != ESP_OK)
    {
        spi_bus_free(W25Q80_ESP32_SPI_HOST);
        return -1;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << W25Q80_ESP32_CS_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLED,
    };
    gpio_config(&io_conf);
    gpio_set_level(W25Q80_ESP32_CS_GPIO, 1);

    g_initialized = true;
    return 0;
}

/**
 * @brief  释放 ESP32 SPI 外设资源
 *
 * @return 0: 成功
 */
static int esp32_w25q80_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    spi_bus_remove_device(g_spi_handle);
    spi_bus_free(W25Q80_ESP32_SPI_HOST);
    gpio_reset_pin(W25Q80_ESP32_CS_GPIO);
    g_initialized = false;
    return 0;
}

/**
 * @brief  控制 CS 引脚电平
 *
 * @param  level  0: 拉低（选中）, 1: 拉高（释放）
 */
static void esp32_w25q80_cs_set(uint8_t level)
{
    gpio_set_level(W25Q80_ESP32_CS_GPIO, level);
}

/**
 * @brief  SPI 只写传输（忽略 MISO）
 *
 * @param  data  发送数据缓冲区
 * @param  len   发送字节数
 * @return 0: 成功, -1: 传输失败
 */
static int esp32_w25q80_spi_write(const uint8_t *data, uint16_t len)
{
    esp_err_t ret;

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    ret = spi_device_polling_transmit(g_spi_handle, &trans);
    return (ret == ESP_OK) ? 0 : -1;
}

/**
 * @brief  SPI 只读传输（MOSI 自动发 0xFF）
 *
 * @param  data  接收数据缓冲区
 * @param  len   接收字节数
 * @return 0: 成功, -1: 传输失败
 */
static int esp32_w25q80_spi_read(uint8_t *data, uint16_t len)
{
    esp_err_t ret;

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = NULL,
        .rx_buffer = data,
    };

    ret = spi_device_polling_transmit(g_spi_handle, &trans);
    return (ret == ESP_OK) ? 0 : -1;
}

/**
 * @brief  SPI 全双工传输（同时发送和接收）
 *
 * @param  tx   发送数据缓冲区
 * @param  rx   接收数据缓冲区
 * @param  len  传输字节数
 * @return 0: 成功, -1: 传输失败
 */
static int esp32_w25q80_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    esp_err_t ret;

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    ret = spi_device_polling_transmit(g_spi_handle, &trans);
    return (ret == ESP_OK) ? 0 : -1;
}

/**
 * @brief  微秒延时
 *
 * @param  us  微秒数
 */
static void esp32_w25q80_delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

/**
 * @brief  毫秒延时（FreeRTOS 阻塞）
 *
 * @param  ms  毫秒数
 */
static void esp32_w25q80_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
