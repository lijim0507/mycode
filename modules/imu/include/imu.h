

#ifndef __IMU_H_
#define __IMU_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct {
    float accel[3];
    float gyro[3];
} imu_data_t;

typedef int (*imu_spi_send_t)(uint8_t *buf, uint16_t len);
typedef int (*imu_spi_recv_t)(uint8_t *buf, uint16_t len);
typedef int (*imu_set_cs_t)(uint8_t level);
typedef void (*imu_delay_t)(uint32_t ms);

typedef struct imu_driver {
    int      (*init)(void *config);
    imu_spi_send_t   spi_send;
    imu_spi_recv_t   spi_recv;
    imu_set_cs_t     set_cs;
    imu_delay_t      delay_ms;
    int      (*deinit)(void);
} imu_driver_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

int   imu_init(const imu_driver_t *driver, void *port_cfg);
int   imu_deinit(void);
void  imu_get_data(imu_data_t *p_data);
void  imu_get_data_task(void);
imu_data_t *imu_get_data_ptr(void);

float   imu_get_accel_x(void);
float   imu_get_accel_y(void);
float   imu_get_accel_z(void);
float   imu_get_gyro_x(void);
float   imu_get_gyro_y(void);
float   imu_get_gyro_z(void);
int16_t imu_get_accel_x_raw(void);
int16_t imu_get_accel_y_raw(void);
int16_t imu_get_accel_z_raw(void);
int16_t imu_get_gyro_x_raw(void);
int16_t imu_get_gyro_y_raw(void);
int16_t imu_get_gyro_z_raw(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
