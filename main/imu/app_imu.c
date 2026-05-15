#include "app_imu.h"
#include "drv_imu.h"
#include "icm42688.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "APP_IMU";

static icm42688_handle_t g_imu_handle;
static imu_data_t g_imu_data;

static err_code_t imu_spi_send_adapter(uint8_t *p_buf, uint16_t len)
{
    return (drv_imu_spi_send(p_buf, len) == IMU_OK) ? ERR_CODE_SUCCESS : ERR_CODE_FAIL;
}

static err_code_t imu_spi_recv_adapter(uint8_t *p_buf, uint16_t len)
{
    return (drv_imu_spi_recv(p_buf, len) == IMU_OK) ? ERR_CODE_SUCCESS : ERR_CODE_FAIL;
}

static err_code_t imu_set_cs_adapter(uint8_t level)
{
    return (drv_imu_set_cs(level) == IMU_OK) ? ERR_CODE_SUCCESS : ERR_CODE_FAIL;
}

/**
 * @brief 初始化并配置ICM42688 IMU传感器
 */
void app_imu_init(void)
{
    icm42688_cfg_t config;
    imu_status_t *p_status = drv_imu_get_status();

    drv_imu_init_mutex();

    p_status->progress = IMU_PROGRESS_SPI_INIT;
    g_imu_handle = icm42688_init();
    if (g_imu_handle == NULL) {
        p_status->spi_init_state = IMU_ERROR;
        p_status->progress = IMU_PROGRESS_ERROR;
        return;
    }
    p_status->spi_init_state = IMU_OK;

    p_status->progress = IMU_PROGRESS_CONFIG;
    config.gyro_mode = ICM42688_GYRO_MODE_LOW_NOISE;
    config.gyro_fs_sel = ICM42688_GFS_SEL_500dps;
    config.gyro_odr = ICM42688_GYRO_ODR_1kHz;
    config.accel_mode = ICM42688_ACCEL_MODE_LOW_NOISE;
    config.accel_fs_sel = ICM42688_ACCEL_FS_SEL_4G;
    config.accel_odr = ICM42688_ACCEL_ODR_1kHz;
    config.accel_bias_x = 0;
    config.accel_bias_y = 0;
    config.accel_bias_z = 0;
    config.gyro_bias_x = 0;
    config.gyro_bias_y = 0;
    config.gyro_bias_z = 0;
    config.comm_mode = ICM42688_COMM_MODE_SPI;
    config.i2c_send = NULL;
    config.i2c_recv = NULL;
    config.spi_send = imu_spi_send_adapter;
    config.spi_recv = imu_spi_recv_adapter;
    config.set_cs = imu_set_cs_adapter;
    config.delay = drv_imu_delay;

    icm42688_set_config(g_imu_handle, config);

    if (icm42688_config(g_imu_handle) == ERR_CODE_SUCCESS) {
        p_status->config_state = IMU_OK;
        p_status->progress = IMU_PROGRESS_READY;
    } else {
        p_status->config_state = IMU_ERROR;
        p_status->progress = IMU_PROGRESS_ERROR;
    }
}

/**
 * @brief 获取ICM42688传感器数据
 * @param p_imu_data 指向imu_data_t结构体的指针，用于存储加速度计和陀螺仪数据
 */
void app_imu_get_data(imu_data_t *p_imu_data)
{
    if (p_imu_data == NULL)
    {
        return;
    }

    icm42688_get_accel_scale(g_imu_handle, &p_imu_data->accel[0], &p_imu_data->accel[1], &p_imu_data->accel[2]);
    icm42688_get_gyro_scale(g_imu_handle, &p_imu_data->gyro[0], &p_imu_data->gyro[1], &p_imu_data->gyro[2]);
}

/**
 * @brief IMU数据获取任务函数
 */
void app_imu_get_data_task(void)
{
    imu_status_t *p_status = drv_imu_get_status();

    p_status->progress = IMU_PROGRESS_READING;
    app_imu_get_data(&g_imu_data);
    p_status->read_state = IMU_OK;
    p_status->progress = IMU_PROGRESS_RUNNING;
}

/**
 * @brief 获取全局IMU数据指针
 * @return imu_data_t* 指向全局IMU数据的指针
 */
imu_data_t* app_imu_get_data_ptr(void)
{
    return &g_imu_data;
}

/**
 * @brief 获取X轴加速度
 * @return float X轴加速度 (g)
 */
float app_imu_get_accel_x(void)
{
    return g_imu_data.accel[0];
}

/**
 * @brief 获取Y轴加速度
 * @return float Y轴加速度 (g)
 */
float app_imu_get_accel_y(void)
{
    return g_imu_data.accel[1];
}

/**
 * @brief 获取Z轴加速度
 * @return float Z轴加速度 (g)
 */
float app_imu_get_accel_z(void)
{
    return g_imu_data.accel[2];
}

/**
 * @brief 获取X轴角速度
 * @return float X轴角速度 (dps)
 */
float app_imu_get_gyro_x(void)
{
    return g_imu_data.gyro[0];
}

/**
 * @brief 获取Y轴角速度
 * @return float Y轴角速度 (dps)
 */
float app_imu_get_gyro_y(void)
{
    return g_imu_data.gyro[1];
}

/**
 * @brief 获取Z轴角速度
 * @return float Z轴角速度 (dps)
 */
float app_imu_get_gyro_z(void)
{
    return g_imu_data.gyro[2];
}

/**
 * @brief 获取X轴原始加速度
 * @return int16_t X轴原始加速度 (ADC值)
 */
int16_t app_imu_get_accel_x_raw(void)
{
    int16_t raw_x, raw_y, raw_z;
    icm42688_get_accel_raw(g_imu_handle, &raw_x, &raw_y, &raw_z);
    return raw_x;
}

/**
 * @brief 获取Y轴原始加速度
 * @return int16_t Y轴原始加速度 (ADC值)
 */
int16_t app_imu_get_accel_y_raw(void)
{
    int16_t raw_x, raw_y, raw_z;
    icm42688_get_accel_raw(g_imu_handle, &raw_x, &raw_y, &raw_z);
    return raw_y;
}

/**
 * @brief 获取Z轴原始加速度
 * @return int16_t Z轴原始加速度 (ADC值)
 */
int16_t app_imu_get_accel_z_raw(void)
{
    int16_t raw_x, raw_y, raw_z;
    icm42688_get_accel_raw(g_imu_handle, &raw_x, &raw_y, &raw_z);
    return raw_z;
}

/**
 * @brief 获取X轴原始角速度
 * @return int16_t X轴原始角速度 (ADC值)
 */
int16_t app_imu_get_gyro_x_raw(void)
{
    int16_t raw_x, raw_y, raw_z;
    icm42688_get_gyro_raw(g_imu_handle, &raw_x, &raw_y, &raw_z);
    return raw_x;
}

/**
 * @brief 获取Y轴原始角速度
 * @return int16_t Y轴原始角速度 (ADC值)
 */
int16_t app_imu_get_gyro_y_raw(void)
{
    int16_t raw_x, raw_y, raw_z;
    icm42688_get_gyro_raw(g_imu_handle, &raw_x, &raw_y, &raw_z);
    return raw_y;
}

/**
 * @brief 获取Z轴原始角速度
 * @return int16_t Z轴原始角速度 (ADC值)
 */
int16_t app_imu_get_gyro_z_raw(void)
{
    int16_t raw_x, raw_y, raw_z;
    icm42688_get_gyro_raw(g_imu_handle, &raw_x, &raw_y, &raw_z);
    return raw_z;
}
