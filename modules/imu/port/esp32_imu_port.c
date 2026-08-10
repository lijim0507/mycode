/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "imu_port.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include <stdbool.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#ifndef IMU_ESP32_SPI_HOST
#define IMU_ESP32_SPI_HOST          SPI2_HOST
#endif

#ifndef IMU_ESP32_MISO_PIN
#define IMU_ESP32_MISO_PIN          GPIO_NUM_12
#endif

#ifndef IMU_ESP32_MOSI_PIN
#define IMU_ESP32_MOSI_PIN          GPIO_NUM_13
#endif

#ifndef IMU_ESP32_SCLK_PIN
#define IMU_ESP32_SCLK_PIN          GPIO_NUM_11
#endif

#ifndef IMU_ESP32_CS_PIN
#define IMU_ESP32_CS_PIN            GPIO_NUM_15
#endif

#ifndef IMU_ESP32_CLOCK_HZ
#define IMU_ESP32_CLOCK_HZ          (10 * 1000 * 1000)
#endif

#ifndef IMU_ESP32_SPI_MODE
#define IMU_ESP32_SPI_MODE          0
#endif

#ifndef IMU_ESP32_QUEUE_SIZE
#define IMU_ESP32_QUEUE_SIZE        7
#endif

#ifndef IMU_ESP32_MAX_TRANSFER_SIZE
#define IMU_ESP32_MAX_TRANSFER_SIZE 32
#endif

#define TAG "IMU_PORT"

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct
{
    spi_device_handle_t spi_handle;
    SemaphoreHandle_t   spi_mutex;
    spi_host_device_t   spi_host;
    gpio_num_t          cs_pin;
    bool                bus_owned;
} imu_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  esp32_imu_init(void);
static int  esp32_imu_deinit(void);
static void esp32_imu_cs_set(uint8_t level);
static int  esp32_imu_spi_write(const uint8_t *data, uint16_t len);
static int  esp32_imu_spi_read(uint8_t *data, uint16_t len);
static int  esp32_imu_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void esp32_imu_delay_us(uint32_t us);
static void esp32_imu_delay_ms(uint32_t ms);
static void esp32_imu_cleanup(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static imu_esp32_ctx_t g_ctx;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  获取平台 SPI 驱动实例（符合 spi_port_driver_t 标准接口）
 * @return 指向静态 const spi_port_driver_t 的指针
 */
const spi_port_driver_t *imu_port_get_driver(void)
{
    static const spi_port_driver_t driver = {
        .init           = esp32_imu_init,
        .deinit         = esp32_imu_deinit,
        .cs_set         = esp32_imu_cs_set,
        .spi_write      = esp32_imu_spi_write,
        .spi_read       = esp32_imu_spi_read,
        .spi_transceive = esp32_imu_spi_transceive,
        .delay_us       = esp32_imu_delay_us,
        .delay_ms       = esp32_imu_delay_ms,
    };
    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 ESP32 SPI 外设和 CS GPIO
 *
 * 所有硬件参数由文件顶部的 #ifndef 宏定义，可在编译时覆盖。
 *
 * @return 0: 成功, -1: mutex 创建失败, -2: GPIO 配置失败,
 *         -3: SPI 总线初始化失败, -4: 设备添加失败
 */
static int esp32_imu_init(void)
{
    esp_err_t ret;

    if (g_ctx.spi_mutex == NULL)
    {
        g_ctx.spi_mutex = xSemaphoreCreateMutex();
        if (g_ctx.spi_mutex == NULL)
        {
            ESP_LOGE(TAG, "mutex create failed");
            return -1;
        }
    }

    if (g_ctx.spi_handle != NULL)
    {
        return 0;
    }

    g_ctx.spi_host = IMU_ESP32_SPI_HOST;
    g_ctx.cs_pin   = IMU_ESP32_CS_PIN;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << g_ctx.cs_pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        esp32_imu_cleanup();
        return -2;
    }
    gpio_set_level(g_ctx.cs_pin, 1);

    spi_bus_config_t bus_cfg = {
        .miso_io_num     = IMU_ESP32_MISO_PIN,
        .mosi_io_num     = IMU_ESP32_MOSI_PIN,
        .sclk_io_num     = IMU_ESP32_SCLK_PIN,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = IMU_ESP32_MAX_TRANSFER_SIZE,
    };

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = IMU_ESP32_CLOCK_HZ,
        .mode           = IMU_ESP32_SPI_MODE,
        .spics_io_num   = -1,
        .queue_size     = IMU_ESP32_QUEUE_SIZE,
        .flags          = 0,
        .pre_cb         = NULL,
        .post_cb        = NULL,
    };

    ret = spi_bus_initialize(g_ctx.spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK)
    {
        g_ctx.bus_owned = true;
    }
    else if (ret != ESP_ERR_INVALID_STATE)
    {
        esp32_imu_cleanup();
        return -3;
    }

    ret = spi_bus_add_device(g_ctx.spi_host, &dev_cfg, &g_ctx.spi_handle);
    if (ret != ESP_OK)
    {
        esp32_imu_cleanup();
        return -4;
    }

    return 0;
}

/**
 * @brief  释放 ESP32 SPI 外设资源
 * @return 0: 成功
 */
static int esp32_imu_deinit(void)
{
    esp32_imu_cleanup();
    return 0;
}

/**
 * @brief  控制 CS 引脚（带 mutex 保护，cs_set(0) 加锁，cs_set(1) 解锁）
 * @param  level  0: 选中, 1: 释放
 */
static void esp32_imu_cs_set(uint8_t level)
{
    if (level == 0 && g_ctx.spi_mutex)
    {
        if (xSemaphoreTake(g_ctx.spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        {
            ESP_LOGW(TAG, "mutex timeout before spi transaction");
            return;
        }
    }

    gpio_set_level(g_ctx.cs_pin, level ? 1 : 0);

    if (level != 0 && g_ctx.spi_mutex)
    {
        xSemaphoreGive(g_ctx.spi_mutex);
    }
}

/**
 * @brief  SPI 只写传输
 */
static int esp32_imu_spi_write(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0)
    {
        return -1;
    }

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    return (spi_device_transmit(g_ctx.spi_handle, &trans) == ESP_OK) ? 0 : -3;
}

/**
 * @brief  SPI 只读传输
 */
static int esp32_imu_spi_read(uint8_t *data, uint16_t len)
{
    if (!data || len == 0)
    {
        return -1;
    }

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = NULL,
        .rx_buffer = data,
        .flags     = 0,
    };

    return (spi_device_transmit(g_ctx.spi_handle, &trans) == ESP_OK) ? 0 : -3;
}

/**
 * @brief  SPI 全双工传输
 */
static int esp32_imu_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    if (!tx || !rx || len == 0)
    {
        return -1;
    }

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    return (spi_device_transmit(g_ctx.spi_handle, &trans) == ESP_OK) ? 0 : -3;
}

/**
 * @brief  微秒延时
 */
static void esp32_imu_delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

/**
 * @brief  毫秒延时
 */
static void esp32_imu_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief  清理 SPI 资源
 */
static void esp32_imu_cleanup(void)
{
    if (g_ctx.spi_handle)
    {
        spi_bus_remove_device(g_ctx.spi_handle);
        g_ctx.spi_handle = NULL;
    }

    if (g_ctx.bus_owned)
    {
        spi_bus_free(g_ctx.spi_host);
        g_ctx.bus_owned = false;
    }

    if (g_ctx.spi_mutex)
    {
        vSemaphoreDelete(g_ctx.spi_mutex);
        g_ctx.spi_mutex = NULL;
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
