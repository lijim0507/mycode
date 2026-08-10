/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "imu.h"
#include "imu_port.h"
#include "icm42688.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static err_code_t imu_spi_send_adapter(uint8_t *buf, uint16_t len);
static err_code_t imu_spi_recv_adapter(uint8_t *buf, uint16_t len);
static err_code_t imu_set_cs_adapter(uint8_t level);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const spi_port_driver_t *g_spi;
static bool                      g_initialized;
static icm42688_handle_t         g_handle;
static imu_data_t                g_data;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 IMU 模块
 *
 * 内部调用 spi_port_get_driver() 获取平台 SPI 驱动并在 port init 之后
 * 完成 ICM-42688 的初始化和配置。
 *
 * @return 0: 成功, -1: spi driver 无效, -2: SPI 硬件初始化失败,
 *         -3: 芯片句柄分配失败, -4: 芯片配置失败
 */
int imu_init(void)
{
	const spi_port_driver_t *driver;

	driver = imu_port_get_driver();
	if (!driver)
	{
		return -1;
	}

	if (!driver->spi_write || !driver->spi_read ||
	    !driver->cs_set || !driver->delay_ms || !driver->init)
	{
		return -1;
	}

	if (g_initialized)
	{
		imu_deinit();
	}

	if (driver->init() != 0)
	{
		return -2;
	}

	g_spi = driver;

	g_handle = icm42688_init();
	if (g_handle == NULL)
	{
		if (g_spi->deinit)
		{
			g_spi->deinit();
		}
		g_spi = NULL;
		return -3;
	}

	icm42688_cfg_t config = {
		.gyro_mode     = ICM42688_GYRO_MODE_LOW_NOISE,
		.gyro_fs_sel   = ICM42688_GFS_SEL_500dps,
		.gyro_odr      = ICM42688_GYRO_ODR_1kHz,
		.accel_mode    = ICM42688_ACCEL_MODE_LOW_NOISE,
		.accel_fs_sel  = ICM42688_ACCEL_FS_SEL_4G,
		.accel_odr     = ICM42688_ACCEL_ODR_1kHz,
		.accel_bias_x  = 0,
		.accel_bias_y  = 0,
		.accel_bias_z  = 0,
		.gyro_bias_x   = 0,
		.gyro_bias_y   = 0,
		.gyro_bias_z   = 0,
		.comm_mode     = ICM42688_COMM_MODE_SPI,
		.i2c_send      = NULL,
		.i2c_recv      = NULL,
		.spi_send      = imu_spi_send_adapter,
		.spi_recv      = imu_spi_recv_adapter,
		.set_cs        = imu_set_cs_adapter,
		.delay         = driver->delay_ms,
	};

	icm42688_set_config(g_handle, config);

	if (icm42688_config(g_handle) != ERR_CODE_SUCCESS)
	{
		free(g_handle);
		g_handle = NULL;
		if (g_spi->deinit)
		{
			g_spi->deinit();
		}
		g_spi = NULL;
		return -4;
	}

	g_initialized = true;
	return 0;
}

int imu_deinit(void)
{
	if (!g_initialized)
	{
		return 0;
	}

	if (g_spi && g_spi->deinit)
	{
		g_spi->deinit();
	}

	free(g_handle);
	g_handle      = NULL;
	g_spi         = NULL;
	g_initialized = false;
	return 0;
}

void imu_get_data(imu_data_t *p_data)
{
	if (!p_data)
	{
		return;
	}

	icm42688_get_accel_scale(g_handle, &p_data->accel[0], &p_data->accel[1], &p_data->accel[2]);
	icm42688_get_gyro_scale(g_handle, &p_data->gyro[0], &p_data->gyro[1], &p_data->gyro[2]);
}

void imu_get_data_task(void)
{
	imu_get_data(&g_data);
}

imu_data_t *imu_get_data_ptr(void)
{
	return &g_data;
}

float imu_get_accel_x(void) { return g_data.accel[0]; }
float imu_get_accel_y(void) { return g_data.accel[1]; }
float imu_get_accel_z(void) { return g_data.accel[2]; }
float imu_get_gyro_x(void)  { return g_data.gyro[0]; }
float imu_get_gyro_y(void)  { return g_data.gyro[1]; }
float imu_get_gyro_z(void)  { return g_data.gyro[2]; }

int16_t imu_get_accel_x_raw(void)
{
	int16_t rx, ry, rz;
	icm42688_get_accel_raw(g_handle, &rx, &ry, &rz);
	return rx;
}

int16_t imu_get_accel_y_raw(void)
{
	int16_t rx, ry, rz;
	icm42688_get_accel_raw(g_handle, &rx, &ry, &rz);
	return ry;
}

int16_t imu_get_accel_z_raw(void)
{
	int16_t rx, ry, rz;
	icm42688_get_accel_raw(g_handle, &rx, &ry, &rz);
	return rz;
}

int16_t imu_get_gyro_x_raw(void)
{
	int16_t rx, ry, rz;
	icm42688_get_gyro_raw(g_handle, &rx, &ry, &rz);
	return rx;
}

int16_t imu_get_gyro_y_raw(void)
{
	int16_t rx, ry, rz;
	icm42688_get_gyro_raw(g_handle, &rx, &ry, &rz);
	return ry;
}

int16_t imu_get_gyro_z_raw(void)
{
	int16_t rx, ry, rz;
	icm42688_get_gyro_raw(g_handle, &rx, &ry, &rz);
	return rz;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  SPI 发送适配器（int → err_code_t）
 */
static err_code_t imu_spi_send_adapter(uint8_t *buf, uint16_t len)
{
	if (g_spi && g_spi->spi_write)
	{
		return (g_spi->spi_write(buf, len) == 0) ? ERR_CODE_SUCCESS : ERR_CODE_FAIL;
	}
	return ERR_CODE_FAIL;
}

/**
 * @brief  SPI 接收适配器（int → err_code_t）
 */
static err_code_t imu_spi_recv_adapter(uint8_t *buf, uint16_t len)
{
	if (g_spi && g_spi->spi_read)
	{
		return (g_spi->spi_read(buf, len) == 0) ? ERR_CODE_SUCCESS : ERR_CODE_FAIL;
	}
	return ERR_CODE_FAIL;
}

/**
 * @brief  SPI CS 控制适配器（int → err_code_t）
 */
static err_code_t imu_set_cs_adapter(uint8_t level)
{
	if (g_spi && g_spi->cs_set)
	{
		g_spi->cs_set(level);
		return ERR_CODE_SUCCESS;
	}
	return ERR_CODE_FAIL;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
