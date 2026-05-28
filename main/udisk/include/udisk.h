

#ifndef __UDISK_H_
#define __UDISK_H_
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
#define UDISK_OK    0
#define UDISK_ERROR 1

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef void *udisk_file_t;

#pragma pack(1)
typedef struct {
    int16_t  left_motor_angle;
    int16_t  left_motor_speed;
    int16_t  left_motor_torque;
    int16_t  right_motor_angle;
    int16_t  right_motor_speed;
    int16_t  right_motor_torque;
    int16_t  gyro[3];
    int8_t   accel[3];
    uint8_t  mode_status;
    uint32_t time;
    int16_t  left_motor_Iq_measured;
    int16_t  right_motor_Iq_measured;
    int16_t  left_motor_Iq_setpoint;
    int16_t  right_motor_Iq_setpoint;
    float    left_pos_to_alg;
    float    right_pos_to_alg;
    uint8_t  hall_error_flags;
    uint8_t  wear_detect_alert;
    float    speed;
} udisk_storage_data_t;
#pragma pack()

typedef struct udisk_driver {
    int      (*init)(void);
    int      (*deinit)(void);
    int      (*host_status)(void);
    int      (*mount)(void);
    int      (*file_open)(udisk_file_t *file, const char *name, uint8_t mode);
    int      (*file_close)(udisk_file_t file);
    int      (*file_write)(udisk_file_t file, const void *data, uint32_t size);
    int      (*file_read)(udisk_file_t file, void *data, uint32_t size);
    int      (*file_sync)(udisk_file_t file);
    int      (*file_rename)(const char *old_path, const char *new_path);
    int      (*is_file_open)(udisk_file_t file);
    uint32_t (*get_time)(void);
    void     (*delay_ms)(uint32_t ms);
} udisk_driver_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

int  udisk_init(const udisk_driver_t *driver);
int  udisk_deinit(void);
void udisk_task(void *param);
void udisk_create_data_file(void);
void udisk_end_data_file(void);
void udisk_set_file_label(uint8_t label);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
