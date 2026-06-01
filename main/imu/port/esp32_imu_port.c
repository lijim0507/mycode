/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "imu_port.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include <stdbool.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define IMU_SPI_HOST   SPI2_HOST
#define IMU_MISO_PIN   GPIO_NUM_12
#define IMU_MOSI_PIN   GPIO_NUM_13
#define IMU_SCLK_PIN   GPIO_NUM_11
#define IMU_CS_PIN     GPIO_NUM_15
#define IMU_SPI_CLOCK_HZ       (10 * 1000 * 1000)
#define IMU_SPI_MODE           0
#define IMU_SPI_QUEUE_SIZE     7
#define IMU_MAX_TRANSFER_SIZE  32

#define TAG "IMU_PORT"

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct {
    spi_device_handle_t spi_handle;
    SemaphoreHandle_t   spi_mutex;
    spi_host_device_t   spi_host;
    gpio_num_t          cs_pin;
    bool                bus_owned;
} imu_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  esp32_imu_init(void *config);
static int  esp32_imu_deinit(void);
static int  esp32_imu_spi_send(uint8_t *buf, uint16_t len);
static int  esp32_imu_spi_recv(uint8_t *buf, uint16_t len);
static int  esp32_imu_set_cs(uint8_t level);
static void esp32_imu_delay(uint32_t ms);
static void esp32_imu_cleanup(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static imu_esp32_ctx_t g_ctx;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const imu_driver_t *imu_port_get_driver(void)
{
    static const imu_driver_t driver = {
        .init     = esp32_imu_init,
        .deinit   = esp32_imu_deinit,
        .spi_send = esp32_imu_spi_send,
        .spi_recv = esp32_imu_spi_recv,
        .set_cs   = esp32_imu_set_cs,
        .delay_ms = esp32_imu_delay,
    };
    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int esp32_imu_init(void *config)
{
    const imu_port_config_t default_cfg = {
        .spi_host          = IMU_SPI_HOST,
        .miso_pin          = IMU_MISO_PIN,
        .mosi_pin          = IMU_MOSI_PIN,
        .sclk_pin          = IMU_SCLK_PIN,
        .cs_pin            = IMU_CS_PIN,
        .clock_hz          = IMU_SPI_CLOCK_HZ,
        .spi_mode          = IMU_SPI_MODE,
        .queue_size        = IMU_SPI_QUEUE_SIZE,
        .max_transfer_size = IMU_MAX_TRANSFER_SIZE,
    };
    const imu_port_config_t *cfg = (const imu_port_config_t *)config;
    esp_err_t ret;

    if (g_ctx.spi_mutex == NULL) {
        g_ctx.spi_mutex = xSemaphoreCreateMutex();
        if (g_ctx.spi_mutex == NULL) {
            ESP_LOGE(TAG, "mutex create failed");
            return -1;
        }
    }

    if (g_ctx.spi_handle != NULL) {
        return 0;
    }

    if (cfg == NULL) {
        cfg = &default_cfg;
    }

    g_ctx.spi_host = (spi_host_device_t)cfg->spi_host;
    g_ctx.cs_pin   = (gpio_num_t)cfg->cs_pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << g_ctx.cs_pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        esp32_imu_cleanup();
        return -2;
    }
    gpio_set_level(g_ctx.cs_pin, 1);

    spi_bus_config_t bus_cfg = {
        .miso_io_num     = cfg->miso_pin,
        .mosi_io_num     = cfg->mosi_pin,
        .sclk_io_num     = cfg->sclk_pin,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = cfg->max_transfer_size,
    };

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = cfg->clock_hz,
        .mode           = cfg->spi_mode,
        .spics_io_num   = -1,
        .queue_size     = cfg->queue_size,
        .flags          = 0,
        .pre_cb         = NULL,
        .post_cb        = NULL,
    };

    ret = spi_bus_initialize(g_ctx.spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK) {
        g_ctx.bus_owned = true;
    } else if (ret != ESP_ERR_INVALID_STATE) {
        esp32_imu_cleanup();
        return -3;
    }

    ret = spi_bus_add_device(g_ctx.spi_host, &dev_cfg, &g_ctx.spi_handle);
    if (ret != ESP_OK) {
        esp32_imu_cleanup();
        return -4;
    }

    return 0;
}

static int esp32_imu_deinit(void)
{
    esp32_imu_cleanup();
    return 0;
}

static int esp32_imu_spi_send(uint8_t *buf, uint16_t len)
{
    if (!buf || len == 0) {
        return -1;
    }

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = buf,
        .rx_buffer = NULL,
    };

    esp_err_t ret = spi_device_transmit(g_ctx.spi_handle, &trans);

    return (ret == ESP_OK) ? 0 : -3;
}

static int esp32_imu_spi_recv(uint8_t *buf, uint16_t len)
{
    if (!buf || len == 0) {
        return -1;
    }

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = NULL,
        .rx_buffer = buf,
        .flags     = 0,
    };

    esp_err_t ret = spi_device_transmit(g_ctx.spi_handle, &trans);

    return (ret == ESP_OK) ? 0 : -3;
}

static int esp32_imu_set_cs(uint8_t level)
{
    if (level == 0 && g_ctx.spi_mutex) {
        if (xSemaphoreTake(g_ctx.spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "mutex timeout before spi transaction");
            return -1;
        }
    }

    gpio_set_level(g_ctx.cs_pin, level ? 1 : 0);

    if (level != 0 && g_ctx.spi_mutex) {
        xSemaphoreGive(g_ctx.spi_mutex);
    }
    return 0;
}

static void esp32_imu_delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void esp32_imu_cleanup(void)
{
    if (g_ctx.spi_handle) {
        spi_bus_remove_device(g_ctx.spi_handle);
        g_ctx.spi_handle = NULL;
    }

    if (g_ctx.bus_owned) {
        spi_bus_free(g_ctx.spi_host);
        g_ctx.bus_owned = false;
    }

    if (g_ctx.spi_mutex) {
        vSemaphoreDelete(g_ctx.spi_mutex);
        g_ctx.spi_mutex = NULL;
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
