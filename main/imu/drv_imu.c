#include "drv_imu.h"
#include "icm42688.h"
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include <stdio.h>

static const char *TAG = "DRV_IMU";

static spi_device_handle_t spi_imu_handle = NULL;
static SemaphoreHandle_t spi_imu_mutex = NULL;

static imu_status_t g_imu_status = {
    .progress = IMU_PROGRESS_UNINIT,
    .spi_init_state = IMU_OK,
    .config_state = IMU_OK,
    .read_state = IMU_OK
};

/**
 * @brief 初始化SPI互斥锁
 * @note 必须在使用SPI之前调用，通常在app_imu_init()中调用
 */
void drv_imu_init_mutex(void)
{
    if (spi_imu_mutex == NULL) {
        spi_imu_mutex = xSemaphoreCreateMutex();
        if (spi_imu_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create SPI mutex");
        } else {
            ESP_LOGI(TAG, "SPI mutex created successfully");
        }
    }
}

/**
 * @brief 初始化SPI总线
 */
static void drv_imu_spi_init(void)
{
    if (spi_imu_handle != NULL) {
        return;  
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << IMU_CS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(IMU_CS_PIN, 1);  

    spi_bus_config_t bus_cfg = {
        .miso_io_num = IMU_MISO_PIN,
        .mosi_io_num = IMU_MOSI_PIN,
        .sclk_io_num = IMU_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32
    };

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10 * 1000 * 1000,  
        .mode = 0,                            
        .spics_io_num = -1,                   /* 软件CS */
        .queue_size = 7,
        .flags = 0,
        .pre_cb = NULL,
        .post_cb = NULL
    };

    ESP_ERROR_CHECK(spi_bus_initialize(IMU_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    ESP_ERROR_CHECK(spi_bus_add_device(IMU_SPI_HOST, &dev_cfg, &spi_imu_handle));
}

/**
 * @brief 通过SPI发送数据
 * @param p_buf_send 指向要发送数据缓冲区的指针
 * @param len 要发送的数据长度
 * @return imu_state_t 返回IMU_OK表示成功，IMU_ERROR表示失败
 */
imu_state_t drv_imu_spi_send(uint8_t *p_buf_send, uint16_t len)
{
    if (p_buf_send == NULL || len == 0) {
        return IMU_ERROR;
    }

    if (spi_imu_handle == NULL) {
        drv_imu_spi_init();
    }

    /* 获取互斥锁 */
    if (spi_imu_mutex != NULL) {
        if (xSemaphoreTake(spi_imu_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take SPI mutex in send");
            return IMU_TIMEOUT;
        }
    }

    spi_transaction_t trans = {
        .length = len * 8,          
        .tx_buffer = p_buf_send,
        .rx_buffer = NULL
    };

    esp_err_t ret = spi_device_transmit(spi_imu_handle, &trans);

    /* 释放互斥锁 */
    if (spi_imu_mutex != NULL) {
        xSemaphoreGive(spi_imu_mutex);
    }

    if (ret != ESP_OK) {
        return IMU_ERROR;
    }

    return IMU_OK;
}

/**
 * @brief 通过SPI接收数据
 * @param p_buf_recv 指向接收数据缓冲区的指针
 * @param len 要接收的数据长度
 * @return imu_state_t 返回IMU_OK表示成功，IMU_ERROR表示失败
 */
imu_state_t drv_imu_spi_recv(uint8_t *p_buf_recv, uint16_t len)
{
    if (p_buf_recv == NULL || len == 0) {
        return IMU_ERROR;
    }

    if (spi_imu_handle == NULL) {
        drv_imu_spi_init();
    }

    /* 获取互斥锁 */
    if (spi_imu_mutex != NULL) {
        if (xSemaphoreTake(spi_imu_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take SPI mutex in recv");
            return IMU_TIMEOUT;
        }
    }

    spi_transaction_t trans = {
        .length = len * 8,           /* 位长度 */
        .tx_buffer = NULL,
        .rx_buffer = p_buf_recv,
        .flags = 0
    };

    esp_err_t ret = spi_device_transmit(spi_imu_handle, &trans);

    /* 释放互斥锁 */
    if (spi_imu_mutex != NULL) {
        xSemaphoreGive(spi_imu_mutex);
    }

    if (ret != ESP_OK) {
        return IMU_ERROR;
    }

    return IMU_OK;
}

/**
 * @brief 设置SPI片选信号
 * @param level 片选信号电平，ICM42688_CS_ACTIVE(0)表示激活片选，ICM42688_CS_UNACTIVE(1)表示取消片选
 * @return imu_state_t 返回IMU_OK
 * @note 使用软件CS控制，确保在整个SPI传输过程中CS保持低电平
 */
imu_state_t drv_imu_set_cs(uint8_t level)
{
    gpio_set_level(IMU_CS_PIN, level ? 1 : 0);
    return IMU_OK;
}

/**
 * @brief 延时函数封装
 * @param delay_ms 延时时间(毫秒)
 */
void drv_imu_delay(uint32_t delay_ms)
{
    common_delay_ms(delay_ms);
}

/**
 * @brief 获取IMU模块状态
 * @return imu_status_t* 指向IMU状态结构体的指针
 */
imu_status_t* drv_imu_get_status(void)
{
    return &g_imu_status;
}
