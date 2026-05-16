
#ifndef __APP_UDISK_H_
#define __APP_UDISK_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "drv_udisk.h"
#include "main.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/
#pragma pack(1)
typedef struct _udisk_storage_data_t
{
    int16_t left_motor_angle;       /* 左电机角度 */
    int16_t left_motor_speed;       /* 左电机速度 */
    int16_t left_motor_torque;      /* 左电机扭矩 */

    int16_t right_motor_angle;      /* 右电机角度 */
    int16_t right_motor_speed;      /* 右电机速度 */
    int16_t right_motor_torque;     /* 右电机扭矩 */

    int16_t gyro[3];                /* 角速度 [x, y, z] */
    int8_t accel[3];                /* 加速度 [x, y, z] */

    uint8_t mode_status;            /* 模式状态 */

    uint32_t time;                  /* 时间戳 */

	int16_t left_motor_Iq_measured;          /* 左电机Iq测量电流 */
	int16_t right_motor_Iq_measured;         /* 右电机Iq测量电流 */
    int16_t left_motor_Iq_setpoint;          /* 左电机Iq设定电流 */
    int16_t right_motor_Iq_setpoint;         /* 右电机Iq设定电流 */

    float left_pos_to_alg;               /* 左电机位置(控制算法使用) */
    float right_pos_to_alg;              /* 右电机位置(控制算法使用) */
    uint8_t hall_error_flags;          /* 霍尔错误标志 */
    uint8_t wear_detect_alert;         /* 佩戴检测警报 */
    float speed;                       /* 行走速度 */

} udisk_storage_data_t;
#pragma pack()


/****************************************************************************/
/*							Exported Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions								*/
/****************************************************************************/
void app_udisk_init(void);
void app_udisk_task(void *param);
void app_udisk_create_data_file(void);
void app_udisk_end_data_file(void);
void app_udisk_set_file_label(uint8_t label);

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/