/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "imu_port.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "common.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define IMU_SPI_HOST   SPI2_HOST
#define IMU_MISO_PIN   GPIO_NUM_12
#define IMU_MOSI_PIN   GPIO_NUM_13
#define IMU_SCLK_PIN   GPIO_NUM_11
#define IMU_CS_PIN     GPIO_NUM_15

#define TAG "IMU_PORT"

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct {
    spi_device_handle_t spi_handle;
    SemaphoreHandle_t   spi_mutex;
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
    (void)config;

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

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << IMU_CS_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(IMU_CS_PIN, 1);

    spi_bus_config_t bus_cfg = {
        .miso_io_num     = IMU_MISO_PIN,
        .mosi_io_num     = IMU_MOSI_PIN,
        .sclk_io_num     = IMU_SCLK_PIN,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 32,
    };

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode           = 0,
        .spics_io_num   = -1,
        .queue_size     = 7,
        .flags          = 0,
        .pre_cb         = NULL,
        .post_cb        = NULL,
    };

    if (spi_bus_initialize(IMU_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        return -2;
    }

    if (spi_bus_add_device(IMU_SPI_HOST, &dev_cfg, &g_ctx.spi_handle) != ESP_OK) {
        return -3;
    }

    return 0;
}

static int esp32_imu_deinit(void)
{
    if (g_ctx.spi_mutex) {
        vSemaphoreDelete(g_ctx.spi_mutex);
        g_ctx.spi_mutex = NULL;
    }
    return 0;
}

static int esp32_imu_spi_send(uint8_t *buf, uint16_t len)
{
    if (!buf || len == 0) {
        return -1;
    }

    if (g_ctx.spi_mutex) {
        if (xSemaphoreTake(g_ctx.spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "mutex timeout in send");
            return -2;
        }
    }

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = buf,
        .rx_buffer = NULL,
    };

    esp_err_t ret = spi_device_transmit(g_ctx.spi_handle, &trans);

    if (g_ctx.spi_mutex) {
        xSemaphoreGive(g_ctx.spi_mutex);
    }

    return (ret == ESP_OK) ? 0 : -3;
}

static int esp32_imu_spi_recv(uint8_t *buf, uint16_t len)
{
    if (!buf || len == 0) {
        return -1;
    }

    if (g_ctx.spi_mutex) {
        if (xSemaphoreTake(g_ctx.spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "mutex timeout in recv");
            return -2;
        }
    }

    spi_transaction_t trans = {
        .length    = len * 8,
        .tx_buffer = NULL,
        .rx_buffer = buf,
        .flags     = 0,
    };

    esp_err_t ret = spi_device_transmit(g_ctx.spi_handle, &trans);

    if (g_ctx.spi_mutex) {
        xSemaphoreGive(g_ctx.spi_mutex);
    }

    return (ret == ESP_OK) ? 0 : -3;
}

static int esp32_imu_set_cs(uint8_t level)
{
    gpio_set_level(IMU_CS_PIN, level ? 1 : 0);
    return 0;
}

static void esp32_imu_delay(uint32_t ms)
{
    common_delay_ms(ms);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
