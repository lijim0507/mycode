/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "encoder.h"
#include "encoder_port.h"

#include <math.h>
#include <string.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define ENCODER_TWO_PI      (6.28318530717958647692f) /* 2*pi */
#define ENCODER_MAX_DT_US   (1000000ULL)              /* 超过 1 s 视为采样中断 */

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/
typedef struct
{
    uint32_t counts_per_rev;    /* 计数/圈，必须大于 0 */
    bool     invert;            /* 翻转计数方向 */
    float    velocity_lpf_hz;   /* 速度低通截止频率，0 表示关闭 */
    float    velocity_deadband; /* 速度死区（counts/s），0 表示关闭 */
    bool     zero_on_index;     /* 首次 Z 脉冲时清零 */
} encoder_config_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void encoder_update_scaled(void);
static void encoder_apply_index(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const encoder_port_driver_t *g_driver;       /* port 驱动实例 */
static encoder_config_t             g_config;       /* 当前模块配置 */
static encoder_data_t               g_data;         /* 对外数据快照 */
static int32_t                      g_prev_raw;     /* 上次原始计数 */
static uint64_t                     g_prev_us;      /* 上次采样时间（us） */
static uint32_t                     g_last_index;   /* 上次 Z 脉冲计数 */
static float                        g_velocity;     /* 滤波后的速度（counts/s） */
static bool                         g_index_found;  /* 是否已见过 Z 脉冲 */
static bool                         g_index_zeroed; /* 首次 Z 是否已清零 */
static bool                         g_initialized;  /* 模块是否已初始化 */

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief 初始化编码器模块
 *
 * @return 0: 成功, -1: 配置无效, -2: port 驱动无效, -3: port 初始化失败
 * @note 模块为单实例，重复调用会先反初始化；配置取自 encoder_config.h
 * @note counts_per_rev 必须大于 0，否则返回 -1
 */
int encoder_init(void)
{
    encoder_config_t config;

    config.counts_per_rev = (uint32_t)ENCODER_DEFAULT_COUNTS_PER_REV;
    config.invert = (bool)ENCODER_DEFAULT_INVERT;
    config.velocity_lpf_hz = (float)ENCODER_DEFAULT_VELOCITY_LPF_HZ;
    config.velocity_deadband = (float)ENCODER_DEFAULT_VELOCITY_DEADBAND;
    config.zero_on_index = (bool)ENCODER_DEFAULT_ZERO_ON_INDEX;

    /* CPR 必须有效，滤波参数不能为负 */
    if (config.counts_per_rev == 0U ||
        config.velocity_lpf_hz < 0.0f ||
        config.velocity_deadband < 0.0f)
    {
        return -1;
    }

    /* 已在运行则先释放旧实例 */
    if (g_initialized)
    {
        encoder_deinit();
    }

    /* port 必须提供初始化、读计数和读时间三个最小接口 */
    g_driver = encoder_port_get_driver();
    if (g_driver == NULL || g_driver->init == NULL || g_driver->read_count == NULL ||
        g_driver->get_time_us == NULL)
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

    g_config = config;
    memset(&g_data, 0, sizeof(g_data));

    /* 用当前硬件计数和时间为差分基准 */
    g_prev_raw = g_driver->read_count();
    g_prev_us = g_driver->get_time_us();
    g_velocity = 0.0f;
    g_index_found = false;
    g_index_zeroed = false;

    /* 记录当前 Z 计数，后续只处理新增脉冲 */
    g_last_index = 0;
    if (g_driver->get_index_count != NULL)
    {
        g_last_index = g_driver->get_index_count();
    }

    g_data.timestamp_us = g_prev_us;
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
    memset(&g_config, 0, sizeof(g_config));
    memset(&g_data, 0, sizeof(g_data));
    g_prev_raw = 0;
    g_prev_us = 0;
    g_last_index = 0;
    g_velocity = 0.0f;
    g_index_found = false;
    g_index_zeroed = false;
    g_initialized = false;
    return 0;
}

/**
 * @brief 采样一次并更新计数、速度、方向
 */
void encoder_update(void)
{
    int32_t raw;
    int32_t delta;
    uint64_t now;
    uint64_t dt_us;

    /* 未初始化时忽略 */
    if (!g_initialized || g_driver == NULL)
    {
        return;
    }

    /* 读取原始计数和时间，差值按 32 位回绕处理 */
    raw = g_driver->read_count();
    now = g_driver->get_time_us();
    delta = (int32_t)((uint32_t)raw - (uint32_t)g_prev_raw);

    /* 安装方向相反时在这里统一翻转 */
    if (g_config.invert)
    {
        delta = -delta;
    }
    g_data.position_count += (int64_t)delta;

    /* 采样间隔异常时只保留位置，不更新速度 */
    dt_us = now - g_prev_us;
    if (dt_us > 0U)
    {
        /* 间隔过长，速度视为无效 */
        if (dt_us > ENCODER_MAX_DT_US)
        {
            g_velocity = 0.0f;
        }
        else
        {
            float dt_s = (float)dt_us * 1e-6f;
            float velocity = (float)delta / dt_s;

            /* 一阶低通，按实际 dt 计算系数 */
            if (g_config.velocity_lpf_hz > 0.0f)
            {
                float tau = 1.0f / (ENCODER_TWO_PI * g_config.velocity_lpf_hz);
                float alpha = dt_s / (tau + dt_s);

                g_velocity += alpha * (velocity - g_velocity);
            }
            else
            {
                /* 未启用滤波，直接使用差分值 */
                g_velocity = velocity;
            }

            /* 低于死区吸附到 0，抑制静止抖动 */
            if (g_config.velocity_deadband > 0.0f &&
                fabsf(g_velocity) < g_config.velocity_deadband)
            {
                g_velocity = 0.0f;
            }
        }
    }

    /* 先处理 Z 吸附/清零，再换算位置 */
    encoder_apply_index();
    encoder_update_scaled();

    g_data.velocity_counts_s = g_velocity;
    g_data.velocity_rad_s = g_velocity * ENCODER_TWO_PI / (float)g_config.counts_per_rev;

    /* 方向取滤波后速度的符号，静止时为 0 */
    if (g_velocity > 0.0f)
    {
        g_data.direction = 1;
    }
    else if (g_velocity < 0.0f)
    {
        g_data.direction = -1;
    }
    else
    {
        g_data.direction = 0;
    }
    g_data.timestamp_us = now;

    g_prev_raw = raw;
    g_prev_us = now;
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
    return g_data.position_count;
}

/**
 * @brief 获取连续多圈绝对角度
 *
 * @return 角度（rad，可正可负）
 */
float encoder_get_position_abs_rad(void)
{
    return g_data.position_abs_rad;
}

/**
 * @brief 获取单圈相对角度
 *
 * @return 角度（rad，范围 [0, 2*pi)）
 */
float encoder_get_position_rel_rad(void)
{
    return g_data.position_rel_rad;
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
 * @brief 获取当前方向
 *
 * @return -1: 反向, 0: 静止, 1: 正向
 */
int8_t encoder_get_direction(void)
{
    return g_data.direction;
}

/**
 * @brief 查询是否已检测到 Z 脉冲
 *
 * @return true: 已检测到, false: 未检测到或不支持 Z
 */
bool encoder_index_found(void)
{
    return g_index_found;
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
    g_data.position_count = 0;
    g_data.velocity_counts_s = 0.0f;
    g_data.velocity_rad_s = 0.0f;
    g_data.direction = 0;
    g_velocity = 0.0f;

    /* 重设差分基准，避免下一拍速度尖峰 */
    g_prev_raw = g_driver->read_count();
    g_prev_us = g_driver->get_time_us();
    g_data.timestamp_us = g_prev_us;
    encoder_update_scaled();
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief 根据累计计数更新位置和单圈角度
 */
static void encoder_update_scaled(void)
{
    int64_t cpr;
    int64_t rem;

    cpr = (int64_t)g_config.counts_per_rev;
    g_data.position_abs_rad = (float)g_data.position_count * ENCODER_TWO_PI / (float)cpr;

    /* 单圈角度用余数计算，避免大计数丢精度 */
    rem = g_data.position_count % cpr;
    if (rem < 0)
    {
        /* 负余数折算到 [0, CPR) */
        rem += cpr;
    }
    g_data.position_rel_rad = (float)rem * ENCODER_TWO_PI / (float)cpr;
}

/**
 * @brief 处理 Z 脉冲，吸附或清零累计计数
 */
static void encoder_apply_index(void)
{
    uint32_t index;
    int64_t cpr;

    /* 没有 Z 相或 port 未实现 Z 计数 */
    if (g_driver == NULL || g_driver->get_index_count == NULL)
    {
        return;
    }

    index = g_driver->get_index_count();

    /* 没有新的 Z 脉冲 */
    if (index == g_last_index)
    {
        return;
    }

    g_last_index = index;
    g_index_found = true;

    /* 首次 Z：按配置清零 */
    if (g_config.zero_on_index && !g_index_zeroed)
    {
        g_data.position_count = 0;
        g_index_zeroed = true;
    }
    else
    {
        /* 后续 Z：吸附到最近整圈，消除累计误差 */
        cpr = (int64_t)g_config.counts_per_rev;
        g_data.position_count = (int64_t)llround((double)g_data.position_count / (double)cpr) * cpr;
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
