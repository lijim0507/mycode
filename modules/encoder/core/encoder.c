/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "encoder.h"
#include "encoder_port.h"
#include "utilities.h"

#include <string.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define ENCODER_TWO_PI      (6.28318530717958647692f) /* 2*pi */
#define ENCODER_UPDATE_PERIOD_S ((float)ENCODER_UPDATE_PERIOD_US * 1e-6f)

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void encoder_update_scaled(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const encoder_port_driver_t *g_driver;            /* port 驱动实例 */
static encoder_data_t               g_data;              /* 对外数据快照 */
static int32_t                      g_prev_raw_count;    /* 上次原始计数 */
static float                        g_velocity_counts_s; /* 滤波后的速度（counts/s） */
static bool                         g_initialized;       /* 模块是否已初始化 */

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief 初始化编码器模块
 *
 * @return 0: 成功, -1: 配置无效, -2: port 驱动无效, -3: port 初始化失败
 * @note 模块为单实例，重复调用会先反初始化；配置取自 encoder_config.h
 * @note counts_per_rev 和 update_period_us 必须大于 0，否则返回 -1
 */
int encoder_init(void)
{
    /* CPR/更新周期必须有效，滤波参数不能为负 */
    if ((uint32_t)ENCODER_COUNTS_PER_REV == 0U ||
        (uint32_t)ENCODER_UPDATE_PERIOD_US == 0U ||
        (float)ENCODER_DEFAULT_VELOCITY_LPF_HZ < 0.0f)
    {
        return -1;
    }

    /* 已在运行则先释放旧实例 */
    if (g_initialized)
    {
        encoder_deinit();
    }

    /* port 必须提供初始化和读计数两个最小接口 */
    g_driver = encoder_port_get_driver();
    if (g_driver == NULL || g_driver->init == NULL || g_driver->read_count == NULL)
    {
        g_driver = NULL;
        return -2;
    }

    /* 硬件初始化失败时不保留半成品状态 */
    if (g_driver->init() != 0)
    {
        g_driver = NULL;
        return -3;
    }

    memset(&g_data, 0, sizeof(g_data));

    /* 用当前硬件计数作为差分基准 */
    g_prev_raw_count = g_driver->read_count();
    g_velocity_counts_s = 0.0f;

    encoder_update_scaled();
    g_initialized = true;
    return 0;
}

/**
 * @brief 反初始化编码器模块
 *
 * @return 0: 成功
 */
int encoder_deinit(void)
{
    /* 未初始化时直接返回 */
    if (!g_initialized)
    {
        return 0;
    }

    /* 先释放 port 资源 */
    if (g_driver != NULL && g_driver->deinit != NULL)
    {
        g_driver->deinit();
    }

    /* 清空模块状态 */
    g_driver = NULL;
    memset(&g_data, 0, sizeof(g_data));
    g_prev_raw_count = 0;
    g_velocity_counts_s = 0.0f;
    g_initialized = false;
    return 0;
}

/**
 * @brief 采样一次并更新计数、速度、方向
 */
void encoder_update(void)
{
    int32_t raw_count;
    int32_t delta_counts;
    float velocity_counts_s;
    float alpha;

    /* 未初始化时忽略 */
    if (!g_initialized || g_driver == NULL)
    {
        return;
    }

    /* 读取原始计数，差值按 32 位回绕处理 */
    raw_count = g_driver->read_count();
    delta_counts = (int32_t)((uint32_t)raw_count - (uint32_t)g_prev_raw_count);

    /* 安装方向相反时在这里统一翻转 */
    if (ENCODER_DEFAULT_INVERT)
    {
        delta_counts = -delta_counts;
    }
    g_data.position_multi_turn_count += (int64_t)delta_counts;

    /* 定周期任务，速度直接按宏定义周期计算 */
    velocity_counts_s = (float)delta_counts / ENCODER_UPDATE_PERIOD_S;

    /* 一阶低通，0 Hz 表示关闭 */
    alpha = first_order_lpf_alpha(ENCODER_UPDATE_PERIOD_S, (float)ENCODER_DEFAULT_VELOCITY_LPF_HZ);
    g_velocity_counts_s = first_order_lpf(&g_velocity_counts_s, velocity_counts_s, alpha);

    encoder_update_scaled();

    g_data.velocity_counts_s = g_velocity_counts_s;
    g_data.velocity_rad_s = g_velocity_counts_s * ENCODER_TWO_PI / (float)ENCODER_COUNTS_PER_REV;
    g_data.velocity_rpm = g_velocity_counts_s * 60.0f / (float)ENCODER_COUNTS_PER_REV;

    /* 方向取滤波后速度的符号，静止时为 0 */
    if (g_velocity_counts_s > 0.0f)
    {
        g_data.direction = 1;
    }
    else if (g_velocity_counts_s < 0.0f)
    {
        g_data.direction = -1;
    }
    else
    {
        g_data.direction = 0;
    }

    g_prev_raw_count = raw_count;
}

/**
 * @brief 获取最近一次更新后的数据快照
 *
 * @param p_data 输出数据指针，传 NULL 时忽略
 */
void encoder_get_data(encoder_data_t *p_data)
{
    /* 空指针保护 */
    if (p_data == NULL)
    {
        return;
    }

    /* 未初始化时返回全 0 快照 */
    if (!g_initialized)
    {
        memset(p_data, 0, sizeof(*p_data));
        return;
    }

    *p_data = g_data;
}

/**
 * @brief 获取连续多圈累计计数
 *
 * @return 累计计数
 */
int64_t encoder_get_count(void)
{
    return g_data.position_multi_turn_count;
}

/**
 * @brief 获取单圈计数
 *
 * @return 计数，范围 [0, CPR)
 */
int64_t encoder_get_position_single_turn_count(void)
{
    return g_data.position_single_turn_count;
}

/**
 * @brief 获取连续多圈角度
 *
 * @return 角度（rad，可正可负，未减 offset）
 */
float encoder_get_position_multi_turn_rad(void)
{
    return g_data.position_multi_turn_rad;
}

/**
 * @brief 获取单圈角度
 *
 * @return 角度（rad，范围 [0, 2*pi)，已减 offset）
 */
float encoder_get_position_single_turn_rad(void)
{
    return g_data.position_single_turn_rad;
}

/**
 * @brief 获取速度
 *
 * @return 速度（counts/s）
 */
float encoder_get_velocity_counts_s(void)
{
    return g_data.velocity_counts_s;
}

/**
 * @brief 获取速度
 *
 * @return 速度（rad/s）
 */
float encoder_get_velocity_rad_s(void)
{
    return g_data.velocity_rad_s;
}

/**
 * @brief 获取速度
 *
 * @return 速度（rpm）
 */
float encoder_get_velocity_rpm(void)
{
    return g_data.velocity_rpm;
}

/**
 * @brief 获取当前方向
 *
 * @return -1: 反向, 0: 静止, 1: 正向
 */
int8_t encoder_get_direction(void)
{
    return g_data.direction;
}

/**
 * @brief 将当前位置清零并复位速度、方向
 */
void encoder_reset_position(void)
{
    /* 未初始化时忽略 */
    if (!g_initialized || g_driver == NULL)
    {
        return;
    }

    /* 清零逻辑位置和运动状态 */
    g_data.position_multi_turn_count = 0;
    g_data.velocity_counts_s = 0.0f;
    g_data.velocity_rad_s = 0.0f;
    g_data.velocity_rpm = 0.0f;
    g_data.direction = 0;
    g_velocity_counts_s = 0.0f;

    /* 重设差分基准，避免下一拍速度尖峰 */
    g_prev_raw_count = g_driver->read_count();
    encoder_update_scaled();
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief 根据多圈计数更新单圈计数、多圈角度和单圈角度
 */
static void encoder_update_scaled(void)
{
    int64_t cpr;
    int64_t rel_count;
    int64_t rel_zeroed;

    cpr = (int64_t)ENCODER_COUNTS_PER_REV;

    /* 单圈计数：多圈计数对 CPR 取余，负数折算到 [0, CPR) */
    rel_count = g_data.position_multi_turn_count % cpr;
    if (rel_count < 0)
    {
        rel_count += cpr;
    }
    g_data.position_single_turn_count = rel_count;

    /* 多圈角度：从零点累计，未减 offset */
    g_data.position_multi_turn_rad = (float)g_data.position_multi_turn_count * ENCODER_TWO_PI / (float)cpr;

    /* 单圈角度：减 offset 后折算到 [0, CPR) */
    rel_zeroed = rel_count - (int64_t)ENCODER_DEFAULT_OFFSET_COUNTS;
    rel_zeroed %= cpr;
    if (rel_zeroed < 0)
    {
        rel_zeroed += cpr;
    }
    g_data.position_single_turn_rad = (float)rel_zeroed * ENCODER_TWO_PI / (float)cpr;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
