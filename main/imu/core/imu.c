/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "imu.h"
#include "imu_port.h"
#include "icm42688.h"

#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define TAG "IMU"
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
static const imu_driver_t *g_driver;
static bool                g_initialized;
static icm42688_handle_t   g_handle;
static imu_data_t          g_data;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int imu_init(const imu_driver_t *driver, void *port_cfg)
{
    if (!driver || !driver->spi_send || !driver->spi_recv || !driver->set_cs) {
        return -1;
    }

    if (g_initialized) {
        imu_deinit();
    }

    if (driver->init && driver->init(port_cfg) != 0) {
        ESP_LOGE(TAG, "driver init failed");
        return -2;
    }

    g_driver = driver;

    g_handle = icm42688_init();
    if (g_handle == NULL) {
        ESP_LOGE(TAG, "icm42688 init failed");
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

    if (icm42688_config(g_handle) != ERR_CODE_SUCCESS) {
        ESP_LOGE(TAG, "icm42688 config failed");
        return -4;
    }

    g_initialized = true;
    return 0;
}

int imu_deinit(void)
{
    if (!g_initialized) {
        return 0;
    }

    if (g_driver && g_driver->deinit) {
        g_driver->deinit();
    }

    g_handle      = NULL;
    g_driver      = NULL;
    g_initialized = false;
    return 0;
}

void imu_get_data(imu_data_t *p_data)
{
    if (!p_data) {
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

static err_code_t imu_spi_send_adapter(uint8_t *buf, uint16_t len)
{
    if (g_driver && g_driver->spi_send) {
        return (g_driver->spi_send(buf, len) == 0) ? ERR_CODE_SUCCESS : ERR_CODE_FAIL;
    }
    return ERR_CODE_FAIL;
}

static err_code_t imu_spi_recv_adapter(uint8_t *buf, uint16_t len)
{
    if (g_driver && g_driver->spi_recv) {
        return (g_driver->spi_recv(buf, len) == 0) ? ERR_CODE_SUCCESS : ERR_CODE_FAIL;
    }
    return ERR_CODE_FAIL;
}

static err_code_t imu_set_cs_adapter(uint8_t level)
{
    if (g_driver && g_driver->set_cs) {
        return (g_driver->set_cs(level) == 0) ? ERR_CODE_SUCCESS : ERR_CODE_FAIL;
    }
    return ERR_CODE_FAIL;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
