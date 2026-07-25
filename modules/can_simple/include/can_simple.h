
#ifndef __CAN_SIMPLE_H_
#define __CAN_SIMPLE_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/* 节点 ID */
#define MOTOR_LEFT_NODE_ID  1
#define MOTOR_RIGHT_NODE_ID 2

/* 命令 ID */
#define CAN_CMD_HEARTBEAT             0x01
#define CAN_CMD_ESTOP                 0x02
#define CAN_CMD_GET_MOTOR_ERROR       0x03
#define CAN_CMD_SET_AXIS_STATE        0x07
#define CAN_CMD_GET_ENCODER_EST       0x09
#define CAN_CMD_SET_CTRL_MODE         0x0B
#define CAN_CMD_SET_INPUT_POS         0x0C
#define CAN_CMD_SET_INPUT_VEL         0x0D
#define CAN_CMD_SET_INPUT_TORQUE      0x0E
#define CAN_CMD_SET_VEL_LIMIT         0x0F
#define CAN_CMD_START_ANTICOGGING     0x10
#define CAN_CMD_SET_TRAJ_VEL_LIMIT    0x11
#define CAN_CMD_SET_TRAJ_ACCEL_LIMIT  0x12
#define CAN_CMD_SET_TRAJ_INERTIA      0x13
#define CAN_CMD_GET_IQ                0x14
#define CAN_CMD_GET_TEMP              0x15
#define CAN_CMD_REBOOT                0x16
#define CAN_CMD_GET_BUS_VI            0x17
#define CAN_CMD_CLEAR_ERRORS          0x18
#define CAN_CMD_SET_LINEAR_COUNT      0x19
#define CAN_CMD_SET_POS_GAIN          0x1A
#define CAN_CMD_SET_VEL_GAINS         0x1B
#define CAN_CMD_GET_TORQUES           0x1C
#define CAN_CMD_GET_POWERS            0x1D
#define CAN_CMD_DISABLE_CAN           0x1E
#define CAN_CMD_SAVE_CFG              0x1F

/* 轴状态 */
#define AXIS_STATE_UNDEFINED           0
#define AXIS_STATE_IDLE                1
#define AXIS_STATE_STARTUP_SEQ         2
#define AXIS_STATE_FULL_CALIB          3
#define AXIS_STATE_MOTOR_CALIB         4
#define AXIS_STATE_ENCODER_IDX_SEARCH  6
#define AXIS_STATE_ENCODER_OFFSET_CALIB 7
#define AXIS_STATE_CLOSED_LOOP         8
#define AXIS_STATE_LOCKIN_SPIN         9
#define AXIS_STATE_ENCODER_DIR_FIND    10
#define AXIS_STATE_HOMING              11

/* 控制模式 */
#define CTRL_MODE_VOLTAGE     0
#define CTRL_MODE_TORQUE      1
#define CTRL_MODE_VELOCITY    2
#define CTRL_MODE_POSITION    3

/* 输入模式 */
#define INPUT_MODE_INACTIVE     0
#define INPUT_MODE_PASSTHROUGH  1
#define INPUT_MODE_VEL_RAMP     2
#define INPUT_MODE_POS_FILTER   3
#define INPUT_MODE_TRAP_TRAJ    5
#define INPUT_MODE_TORQUE_RAMP  6
#define INPUT_MODE_MIRROR       9

/* CAN ID 生成宏 */
#define CAN_ID_GEN(node_id, cmd_id)  (((node_id) << 5) | ((cmd_id) & 0x1F))

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct can_simple_driver {
    int  (*init)(void *config);
    int  (*send)(uint16_t id, const uint8_t *data, uint8_t len);
    int  (*recv)(uint16_t *id, uint8_t *data, uint8_t *len, uint32_t timeout_ms);
    int  (*deinit)(void);
} can_simple_driver_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

int  can_simple_init(const can_simple_driver_t *driver, void *port_cfg);
int  can_simple_deinit(void);

void can_simple_set_axis_state(uint8_t node_id, uint8_t axis_state);
void can_simple_set_ctrl_mode(uint8_t node_id, uint8_t ctrl_mode, uint8_t input_mode);
void can_simple_set_input_pos(uint8_t node_id, float pos, int16_t vel_ff, int16_t torque_ff);
void can_simple_set_input_vel(uint8_t node_id, float vel, float torque_ff);
void can_simple_set_input_torque(uint8_t node_id, float torque);
void can_simple_set_vel_limit(uint8_t node_id, float vel_limit);
void can_simple_set_traj_vel_limit(uint8_t node_id, float traj_vel_limit);
void can_simple_set_traj_accel_limit(uint8_t node_id, float traj_accel_limit, float traj_decel_limit);
void can_simple_set_pos_gain(uint8_t node_id, float pos_gain);
void can_simple_set_vel_gains(uint8_t node_id, float vel_gain, float vel_integrator_gain);
void can_simple_reboot(uint8_t node_id);
void can_simple_clear_errors(uint8_t node_id);
void can_simple_estop(uint8_t node_id);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
