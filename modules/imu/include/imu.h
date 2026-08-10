

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
} imu_data_t;  /* float accel[3] + float gyro[3] */

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

/**
 * @brief  初始化 IMU 模块
 *         内部调用 spi_port_get_driver() 获取 SPI 驱动并初始化 ICM-42688。
 * @return 0: 成功, -1: spi driver 无效, -2: SPI 硬件初始化失败,
 *         -3: 芯片句柄分配失败, -4: 芯片配置失败
 */
int   imu_init(void);
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
