#ifndef __DRV_IMU_H
#define __DRV_IMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <stdint.h>

#define IMU_SPI_HOST         SPI2_HOST      // 使用SPI2
#define IMU_MISO_PIN         GPIO_NUM_12    // SPI MISO
#define IMU_MOSI_PIN         GPIO_NUM_13    // SPI MOSI
#define IMU_SCLK_PIN         GPIO_NUM_11    // SPI 时钟
#define IMU_CS_PIN           GPIO_NUM_15    // SPI片选

typedef enum {
    IMU_OK = 0,
    IMU_ERROR = 1,
    IMU_BUSY = 2,
    IMU_TIMEOUT = 3,
    IMU_DATA_INVALID = 4
} imu_state_t;

typedef enum {
    IMU_PROGRESS_UNINIT = 0,
    IMU_PROGRESS_SPI_INIT,
    IMU_PROGRESS_CONFIG,
    IMU_PROGRESS_READY,
    IMU_PROGRESS_READING,
    IMU_PROGRESS_RUNNING,
    IMU_PROGRESS_ERROR
} imu_progress_t;

typedef struct {
    imu_progress_t progress;
    imu_state_t spi_init_state;
    imu_state_t config_state;
    imu_state_t read_state;
} imu_status_t;

void drv_imu_init_mutex(void);
imu_state_t drv_imu_spi_send(uint8_t *p_buf_send, uint16_t len);
imu_state_t drv_imu_spi_recv(uint8_t *p_buf_recv, uint16_t len);
imu_state_t drv_imu_set_cs(uint8_t level);
void drv_imu_delay(uint32_t delay_ms);
imu_status_t* drv_imu_get_status(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_IMU_H */
