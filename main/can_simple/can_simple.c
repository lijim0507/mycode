
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "can_simple.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

// can simple 命令类型定义
#define CAN_CMD_HEARTBEAT 0x01            /* 心跳命令 */
#define CAN_CMD_ESTOP 0x02                /* 急停命令 */
#define CAN_CMD_GET_MOTOR_ERROR 0x03      /* 获取电机错误 */
#define CAN_CMD_SET_AXIS_STATE 0x07       /* 设置轴状态 */
#define CAN_CMD_GET_ENCODER_EST 0x09      /* 获取编码器估计值 */
#define CAN_CMD_SET_CTRL_MODE 0x0B        /* 设置控制器模式 */
#define CAN_CMD_SET_INPUT_POS 0x0C        /* 设置输入位置 */
#define CAN_CMD_SET_INPUT_VEL 0x0D        /* 设置输入速度 */
#define CAN_CMD_SET_INPUT_TORQUE 0x0E     /* 设置输入转矩 */
#define CAN_CMD_SET_VEL_LIMIT 0x0F        /* 设置速度限制 */
#define CAN_CMD_START_ANTICOGGING 0x10    /* 启动反齿槽 */
#define CAN_CMD_SET_TRAJ_VEL_LIMIT 0x11   /* 设置轨迹速度限制 */
#define CAN_CMD_SET_TRAJ_ACCEL_LIMIT 0x12 /* 设置轨迹加速度限制 */
#define CAN_CMD_SET_TRAJ_INERTIA 0x13     /* 设置轨迹惯性 */
#define CAN_CMD_GET_IQ 0x14               /* 获取Iq电流 */
#define CAN_CMD_GET_TEMP 0x15             /* 获取温度 */
#define CAN_CMD_REBOOT 0x16               /* 重启 */
#define CAN_CMD_GET_BUS_VI 0x17           /* 获取母线电压电流 */
#define CAN_CMD_CLEAR_ERRORS 0x18         /* 清除错误 */
#define CAN_CMD_SET_LINEAR_COUNT 0x19     /* 设置线性计数 */
#define CAN_CMD_SET_POS_GAIN 0x1A         /* 设置位置增益 */
#define CAN_CMD_SET_VEL_GAINS 0x1B        /* 设置速度增益 */
#define CAN_CMD_GET_TORQUES 0x1C          /* 获取转矩 */
#define CAN_CMD_GET_POWERS 0x1D           /* 获取功率 */
#define CAN_CMD_DISABLE_CAN 0x1E          /* 禁用CAN */
#define CAN_CMD_SAVE_CFG 0x1F             /* 保存配置 */

#define AXIS_STATE_UNDEFINED 0            /* 未定义 */
#define AXIS_STATE_IDLE 1                 /* 空闲 */
#define AXIS_STATE_STARTUP_SEQ 2          /* 启动序列 */
#define AXIS_STATE_FULL_CALIB 3           /* 完全校准 */
#define AXIS_STATE_MOTOR_CALIB 4          /* 电机校准 */
#define AXIS_STATE_ENCODER_IDX_SEARCH 6   /* 编码器索引搜索 */
#define AXIS_STATE_ENCODER_OFFSET_CALIB 7 /* 编码器偏移校准 */
#define AXIS_STATE_CLOSED_LOOP 8          /* 闭环控制 */
#define AXIS_STATE_LOCKIN_SPIN 9          /* 锁定旋转 */
#define AXIS_STATE_ENCODER_DIR_FIND 10    /* 编码器方向查找 */
#define AXIS_STATE_HOMING 11              /* 归零 */

#define CTRL_MODE_VOLTAGE 0  /* 电压控制模式 */
#define CTRL_MODE_TORQUE 1   /* 转矩控制模式 */
#define CTRL_MODE_VELOCITY 2 /* 速度控制模式 */
#define CTRL_MODE_POSITION 3 /* 位置控制模式 */

#define INPUT_MODE_INACTIVE 0    /* 闲置 */
#define INPUT_MODE_PASSTHROUGH 1 /* 直接控制 */
#define INPUT_MODE_VEL_RAMP 2    /* 速度斜坡 */
#define INPUT_MODE_POS_FILTER 3  /* 位置滤波 */
#define INPUT_MODE_TRAP_TRAJ 5   /* 梯形曲线 */
#define INPUT_MODE_TORQUE_RAMP 6 /* 力矩斜坡 */
#define INPUT_MODE_MIRROR 9      /* 运动控制(MIT) */

#define CAN_ID_GEN(node_id, cmd_id) (((node_id) << 5) | ((cmd_id) & 0x1F))

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/