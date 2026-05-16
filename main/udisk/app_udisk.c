/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "app_udisk.h"
#include "utilities.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define NUMBER_FILE_NAME "config.dat"

#define FILENAME_MAX_SIZE    32      /* 文件名最大长度 */

#ifndef PI
#define PI                      3.14159265358979323846f
#endif

/* USB数据打包范围定义 */
#define UDISK_MOTOR_POSITION_MIN      -PI         /* 电机位置最小值(弧度) */
#define UDISK_MOTOR_POSITION_MAX      PI          /* 电机位置最大值(弧度) */
#define UDISK_MOTOR_SPEED_MIN         -33.0f      /* 电机速度最小值(rad/s) */
#define UDISK_MOTOR_SPEED_MAX         33.0f       /* 电机速度最大值(rad/s) */
#define UDISK_MOTOR_TORQUE_MIN        -17.0f      /* 电机扭矩最小值(Nm) */
#define UDISK_MOTOR_TORQUE_MAX        17.0f       /* 电机扭矩最大值(Nm) */
#define UDISK_MOTOR_IQ_MIN            -50.0f      /* 电机Iq电流最小值(A) */
#define UDISK_MOTOR_IQ_MAX            50.0f       /* 电机Iq电流最大值(A) */
#define UDISK_GYRO_MIN                -500.0f     /* 陀螺仪最小值(dps) */
#define UDISK_GYRO_MAX                500.0f      /* 陀螺仪最大值(dps) */
#define UDISK_ACCEL_MIN               -4.0f       /* 加速度计最小值(g) */
#define UDISK_ACCEL_MAX               4.0f        /* 加速度计最大值(g) */           


#define UDISK_SYNC_COUNT             100          /* 写入多少次后进行一次同步 */

#define LABEL_UPDATE_TIMEOUT    5000         /* 标签更新最小间隔时间(ms) */
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/
//数据文件命名结构体定义
typedef struct _app_data_file_name_t
{
    char chip_id[4];
    char serial_number[4];
    char mode[4];
    char lable[4];
    const char end_character[1]; // 用于确保字符串以null结尾 
}app_data_file_name_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void app_udisk_store_data_pack(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static FIL *p_data_file = NULL;

//存储数据结构体实例
udisk_storage_data_t storage_data;

//当前正常操作的 文件名结构体实例
app_data_file_name_t data_file_name = {
    .chip_id = "0000",
    .serial_number = "0000",
    .mode = "0000",
    .lable = "0000",
    .end_character = '\0'
};
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief 初始化udisk存储模块
 */
void app_udisk_init(void)
{

    uint32_t file_number = 0;
    FIL *p_num_file = NULL;

    drv_udisk_init();

    if (drv_udisk_file_open(&p_num_file, NUMBER_FILE_NAME, FA_OPEN_ALWAYS | FA_WRITE | FA_READ) == UDISK_OK)
    {
        drv_udisk_file_read(&p_num_file, &file_number, sizeof(file_number));
        drv_udisk_file_close(&p_num_file);
    }
    else
    {
        //ESP_LOGE(TAG, "failed to open number file");
    }

}

/**
 * @brief udisk数据任务
 */
void app_udisk_task(void *param)
{
    uint32_t write_count = 0;
    while (1)
    {
        app_udisk_store_data_pack();

        if(1)
        {
            //存储数据到文件
            if(p_data_file != NULL)
            {
                if (drv_udisk_file_write(p_data_file, &storage_data, sizeof(udisk_storage_data_t)) == UDISK_OK)
                {
                    write_count++;
                    drv_udisk_toggle_led();
                }

                if (write_count >= UDISK_SYNC_COUNT)
                {
                    if (drv_udisk_file_sync(p_data_file) == UDISK_OK)
                    {
                        write_count = 0;
                    }
                }
            }
        }
        else
        {
            app_udisk_init();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
}

/**
 * @brief 创建默认的数据文件，chip_id和serial_number按规律赋值，
 *          mode和lable先赋默认值，后续根据状态更新文件名
 * 
 */
void app_udisk_create_data_file(void)
{
    char filename[FILENAME_MAX_SIZE] = {0};
    uint16_t serial_num = 0;
    uint16_t chip_id;


    //关闭之前打开的数据文件（如果有）
    if(p_data_file != NULL)
    {
        drv_udisk_file_close(&p_data_file);
    }

    app_udisk_reset_data_file_name();

    //更新chip id
    chip_id = app_control_get_chip_id();
    snprintf(data_file_name.chip_id, sizeof(data_file_name.chip_id), "%04 d", chip_id);

    //更新文件序列号
    serial_num = app_udisk_get_serial_number();
    snprintf(data_file_name.serial_number, sizeof(data_file_name.serial_number), "%04d", serial_num);

    
    snprintf(filename, FILENAME_MAX_SIZE, "%s_%s_%s_%s%s.dat", 
    data_file_name.chip_id, data_file_name.serial_number, data_file_name.mode, data_file_name.lable, data_file_name.end_character);

    //创建新数据文件并打开
    if (drv_udisk_file_open(&p_data_file, filename, FA_OPEN_APPEND | FA_WRITE | FA_READ) != UDISK_OK)
    {
        //ESP_LOGE(TAG, "failed to create data file");
    }
}


void app_udisk_end_data_file(void)
{
    
    app_udisk_update_data_file_name();

    drv_udisk_file_close(&p_data_file);

    app_udisk_reset_data_file_name();

}

void app_udisk_set_file_label(uint8_t label)
{
    
    static char label_str[4] = {0};
    static uint8_t label_index = 0;
    static uint32_t last_label_update_time = 0;


    if(last_label_update_time == 0 || 
        strcmp(label_str, "0000") == 0 ||
        (drv_udisk_get_time() - last_label_update_time) >= LABEL_UPDATE_TIMEOUT)
    {
        label_str[0] = '0' ;   
        label_str[1] = '0' ;   
        label_str[2] = '0' ;   
        label_str[3] = '0' ;
        label_index = 0;
    }
    else
    {

    }

    label_str[label_index++] = (label) + '0';
    snprintf(data_file_name.lable, sizeof(data_file_name.lable), "%s", label_str);

    last_label_update_time = drv_udisk_get_time();

}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/
static uint16_t app_udisk_get_serial_number()
{
    uint32_t file_number = 0;
    FIL *p_num_file = NULL;

    if (drv_udisk_file_open(&p_num_file, NUMBER_FILE_NAME, FA_READ|FA_WRITE) == UDISK_OK)
    {
        drv_udisk_file_read(&p_num_file, &file_number, sizeof(file_number));
        file_number++;
        drv_udisk_file_write(&p_num_file, &file_number, sizeof(file_number));
        drv_udisk_file_sync(&p_num_file);
        drv_udisk_file_close(&p_num_file);
        return (uint16_t)--file_number;
    }
    else
    {
        //ESP_LOGE(TAG, "failed to open number file for reading");
        return 0;
    }

}
static void app_udisk_reset_data_file_name(void)
{
    snprintf(data_file_name.chip_id, sizeof(data_file_name.chip_id), "0000");
    snprintf(data_file_name.serial_number, sizeof(data_file_name.serial_number), "0000");
    snprintf(data_file_name.mode, sizeof(data_file_name.mode), "0000");
    snprintf(data_file_name.lable, sizeof(data_file_name.lable), "0000");

}

static void app_udisk_update_data_file_name(void)
{
    uint32_t file_number = 0;
    FIL *p_num_file = NULL;
    char old_filename[FILENAME_MAX_SIZE] = {0};
    char new_filename[FILENAME_MAX_SIZE] = {0};
    uint16_t serial_num = 0;

    //获取当前文件序列号    
    snprintf(old_filename, FILENAME_MAX_SIZE, "%s_%s_%s_%s%s.dat", 
    data_file_name.chip_id, data_file_name.serial_number, data_file_name.mode, data_file_name.lable, data_file_name.end_character);


    //更新文件 label label在uart接收到label事件中更新
    // app_udisk_set_file_label(uint8_t label);


    //更新运动模式 mode 
    switch (app_control_get_mode())
    {
    case 1:
        snprintf(data_file_name.mode, sizeof(data_file_name.mode), "%04d", mode);
        break;
    case 2:
        snprintf(data_file_name.mode, sizeof(data_file_name.mode), "%04d", mode);
        break;
    case 3:
        snprintf(data_file_name.mode, sizeof(data_file_name.mode), "%04d", mode);
        break;
    case 4:
        snprintf(data_file_name.mode, sizeof(data_file_name.mode), "%04d", mode);
        break;
    default:
        break;
    }

    //生成新的文件名
    snprintf(new_filename, FILENAME_MAX_SIZE, "%s_%s_%s_%s%s.dat", 
    data_file_name.chip_id, data_file_name.serial_number, data_file_name.mode, data_file_name.lable);

    //重命名数据文件
    if (drv_udisk_file_rename(old_filename, new_filename) != UDISK_OK)
    {
        //ESP_LOGE(TAG, "failed to rename data file");
    }



    return 0;
}

static void app_udisk_store_data_pack(void)
{
    float left_position_f;
    float left_speed_f;
    float left_torque_f;
    float left_Iq_measured_f;
    float left_Iq_setpoint_f;
    float right_position_f;
    float right_speed_f;
    float right_torque_f;
    float right_Iq_measured_f;
    float right_Iq_setpoint_f;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float accel_x;
    float accel_y;
    float accel_z;

    left_position_f = clamp_float(app_control_get_left_pos(), UDISK_MOTOR_POSITION_MIN, UDISK_MOTOR_POSITION_MAX);
    left_speed_f = clamp_float(app_motor_get_left_velocity(), UDISK_MOTOR_SPEED_MIN, UDISK_MOTOR_SPEED_MAX);
    left_torque_f = clamp_float(app_motor_get_left_torque(), UDISK_MOTOR_TORQUE_MIN, UDISK_MOTOR_TORQUE_MAX);
    left_Iq_measured_f = clamp_float(app_motor_get_left_Iq_measured(), UDISK_MOTOR_IQ_MIN, UDISK_MOTOR_IQ_MAX);
    left_Iq_setpoint_f = clamp_float(app_motor_get_left_Iq_setpoint(), UDISK_MOTOR_IQ_MIN, UDISK_MOTOR_IQ_MAX);
    right_position_f = clamp_float(app_control_get_right_pos(), UDISK_MOTOR_POSITION_MIN, UDISK_MOTOR_POSITION_MAX);
    right_speed_f = clamp_float(app_motor_get_right_velocity(), UDISK_MOTOR_SPEED_MIN, UDISK_MOTOR_SPEED_MAX);
    right_torque_f = clamp_float(app_motor_get_right_torque(), UDISK_MOTOR_TORQUE_MIN, UDISK_MOTOR_TORQUE_MAX);
    right_Iq_measured_f = clamp_float(app_motor_get_right_Iq_measured(), UDISK_MOTOR_IQ_MIN, UDISK_MOTOR_IQ_MAX);
    right_Iq_setpoint_f = clamp_float(app_motor_get_right_Iq_setpoint(), UDISK_MOTOR_IQ_MIN, UDISK_MOTOR_IQ_MAX);
    gyro_x = clamp_float(app_imu_get_gyro_x(), UDISK_GYRO_MIN, UDISK_GYRO_MAX);
    gyro_y = clamp_float(app_imu_get_gyro_y(), UDISK_GYRO_MIN, UDISK_GYRO_MAX);
    gyro_z = clamp_float(app_imu_get_gyro_z(), UDISK_GYRO_MIN, UDISK_GYRO_MAX);
    accel_x = clamp_float(app_imu_get_accel_x(), UDISK_ACCEL_MIN, UDISK_ACCEL_MAX);
    accel_y = clamp_float(app_imu_get_accel_y(), UDISK_ACCEL_MIN, UDISK_ACCEL_MAX);
    accel_z = clamp_float(app_imu_get_accel_z(), UDISK_ACCEL_MIN, UDISK_ACCEL_MAX);

    storage_data.left_motor_angle = float_to_int16(left_position_f, UDISK_MOTOR_POSITION_MIN, UDISK_MOTOR_POSITION_MAX);
    storage_data.left_motor_speed = float_to_int16(left_speed_f, UDISK_MOTOR_SPEED_MIN, UDISK_MOTOR_SPEED_MAX);
    storage_data.left_motor_torque = float_to_int16(left_torque_f, UDISK_MOTOR_TORQUE_MIN, UDISK_MOTOR_TORQUE_MAX);
    storage_data.left_motor_Iq_measured = float_to_int16(left_Iq_measured_f, UDISK_MOTOR_IQ_MIN, UDISK_MOTOR_IQ_MAX);
    storage_data.left_motor_Iq_setpoint = float_to_int16(left_Iq_setpoint_f, UDISK_MOTOR_IQ_MIN, UDISK_MOTOR_IQ_MAX);

    storage_data.right_motor_angle = float_to_int16(right_position_f, UDISK_MOTOR_POSITION_MIN, UDISK_MOTOR_POSITION_MAX);
    storage_data.right_motor_speed = float_to_int16(right_speed_f, UDISK_MOTOR_SPEED_MIN, UDISK_MOTOR_SPEED_MAX);
    storage_data.right_motor_torque = float_to_int16(right_torque_f, UDISK_MOTOR_TORQUE_MIN, UDISK_MOTOR_TORQUE_MAX);
    storage_data.right_motor_Iq_measured = float_to_int16(right_Iq_measured_f, UDISK_MOTOR_IQ_MIN, UDISK_MOTOR_IQ_MAX);
    storage_data.right_motor_Iq_setpoint = float_to_int16(right_Iq_setpoint_f, UDISK_MOTOR_IQ_MIN, UDISK_MOTOR_IQ_MAX);

    storage_data.gyro[0] = float_to_int16(gyro_x, UDISK_GYRO_MIN, UDISK_GYRO_MAX);
    storage_data.gyro[1] = float_to_int16(gyro_y, UDISK_GYRO_MIN, UDISK_GYRO_MAX);
    storage_data.gyro[2] = float_to_int16(gyro_z, UDISK_GYRO_MIN, UDISK_GYRO_MAX);

    storage_data.accel[0] = float_to_int8(accel_x, UDISK_ACCEL_MIN, UDISK_ACCEL_MAX);
    storage_data.accel[1] = float_to_int8(accel_y, UDISK_ACCEL_MIN, UDISK_ACCEL_MAX);
    storage_data.accel[2] = float_to_int8(accel_z, UDISK_ACCEL_MIN, UDISK_ACCEL_MAX);

    storage_data.mode_status = (uint8_t)app_control_get_state();
    storage_data.time = drv_udisk_get_tick();
    storage_data.left_pos_to_alg = app_control_get_left_pos_to_alg();
    storage_data.right_pos_to_alg = app_control_get_right_pos_to_alg();
    storage_data.hall_error_flags = 0;
    storage_data.wear_detect_alert = app_control_get_wear_detect_alert();
    storage_data.speed = app_control_get_walk_speed();

}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/

