#ifndef __APP_IMU_H
#define __APP_IMU_H

#ifdef __cplusplus
extern "C" {
#endif

//#include "drv_imu.h"
#include <stdint.h>

typedef struct {
    float accel[3];                 /* 加速度 [x, y, z] (单位: g) */
    float gyro[3];                  /* 角速度 [x, y, z] (单位: dps) */
} imu_data_t;

void app_imu_init(void);
void app_imu_get_data(imu_data_t *p_imu_data);
void app_imu_get_data_task(void);
imu_data_t* app_imu_get_data_ptr(void);

float app_imu_get_accel_x(void);
float app_imu_get_accel_y(void);
float app_imu_get_accel_z(void);

float app_imu_get_gyro_x(void);
float app_imu_get_gyro_y(void);
float app_imu_get_gyro_z(void);

int16_t app_imu_get_accel_x_raw(void);
int16_t app_imu_get_accel_y_raw(void);
int16_t app_imu_get_accel_z_raw(void);

int16_t app_imu_get_gyro_x_raw(void);
int16_t app_imu_get_gyro_y_raw(void);
int16_t app_imu_get_gyro_z_raw(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_IMU_H */
