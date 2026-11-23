#ifndef __ENCODER_H_
#define __ENCODER_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#include "encoder_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief 编码器数据快照
 */
typedef struct
{
    int64_t  position_count;     /* 连续多圈累计计数 */
    float    position_abs_rad;   /* 连续多圈绝对角度（rad，可正可负） */
    float    position_rel_rad;   /* 单圈相对角度 [0, 2*pi)（rad） */
    float    velocity_counts_s;  /* 速度（counts/s） */
    float    velocity_rad_s;     /* 速度（rad/s） */
    int8_t   direction;          /* -1 / 0 / +1 */
    uint64_t timestamp_us;       /* 本次采样时间（us） */
} encoder_data_t;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief 初始化编码器模块
 *
 * @return 0: 成功, -1: 配置无效, -2: port 驱动无效, -3: port 初始化失败
 * @note 模块为单实例，重复调用会先反初始化；配置取自 encoder_config.h
 * @note counts_per_rev 必须大于 0，否则返回 -1
 */
int encoder_init(void);

/**
 * @brief 反初始化编码器模块
 *
 * @return 0: 成功
 */
int encoder_deinit(void);

/**
 * @brief 采样一次并更新计数、速度、方向
 *
 * @note 由调用方周期调用，建议与控制周期一致；update 与读取应在同一上下文，模块内部不加锁
 */
void encoder_update(void);

/**
 * @brief 获取最近一次更新后的数据快照
 *
 * @param p_data 输出数据指针，传 NULL 时忽略
 */
void encoder_get_data(encoder_data_t *p_data);

/**
 * @brief 获取连续多圈累计计数
 *
 * @return 累计计数
 */
int64_t encoder_get_count(void);

/**
 * @brief 获取连续多圈绝对角度
 *
 * @return 角度（rad，可正可负）
 */
float encoder_get_position_abs_rad(void);

/**
 * @brief 获取单圈相对角度
 *
 * @return 角度（rad，范围 [0, 2*pi)）
 */
float encoder_get_position_rel_rad(void);

/**
 * @brief 获取速度
 *
 * @return 速度（counts/s）
 */
float encoder_get_velocity_counts_s(void);

/**
 * @brief 获取速度
 *
 * @return 速度（rad/s）
 */
float encoder_get_velocity_rad_s(void);

/**
 * @brief 获取当前方向
 *
 * @return -1: 反向, 0: 静止, 1: 正向
 */
int8_t encoder_get_direction(void);

/**
 * @brief 查询是否已检测到 Z 脉冲
 *
 * @return true: 已检测到, false: 未检测到或不支持 Z
 */
bool encoder_index_found(void);

/**
 * @brief 将当前位置清零并复位速度、方向
 */
void encoder_reset_position(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
