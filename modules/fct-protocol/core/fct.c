/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "fct.h"
#include "fct_port.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/* 帧头、类型、级别等固定字段的最大长度预留，用于校验发送缓冲是否够用 */
#define FCT_FRAME_RESERVE       48U

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  模块内部测试状态
 */
typedef enum
{
    FCT_STATE_IDLE = 0,     /* 上电或 END 后，允许 START / HB */
    FCT_STATE_TESTING,      /* START 与 RESULT 之间 */
    FCT_STATE_RESULT_SENT,  /* RESULT 之后、END 之前 */
} fct_state_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  fct_send_frame(const char *type, const char *payload);
static void fct_clean_text(char *dst, const char *src);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static const fct_port_driver_t *g_driver;
static bool                     g_initialized;
static fct_state_t              g_state;
static uint32_t                 g_seq;
static uint8_t                  g_tx_buf[FCT_TX_BUF_SIZE];

static const char *const g_log_level_name[] =
{
    "INFO",
    "WARN",
    "ERROR",
    "DEBUG",
};

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 FCT 模块
 *
 * @return 0: 成功, -1: 配置无效, -2: port 驱动无效, -3: 串口初始化失败
 * @note   模块为单实例，重复调用会先反初始化。
 */
int fct_init(void)
{
    const fct_port_driver_t *driver;

    /* 型号 0 保留给未知，发送缓冲必须能容纳最长 LOG */
    if ((uint32_t)FCT_MODEL_TYPE == 0U || (uint32_t)FCT_MODEL_TYPE > 255U)
    {
        return -1;
    }

    if ((uint32_t)FCT_TX_BUF_SIZE <= (uint32_t)FCT_LOG_TEXT_MAX + FCT_FRAME_RESERVE)
    {
        return -1;
    }

    /* 已在运行则先释放旧实例 */
    if (g_initialized)
    {
        fct_deinit();
    }

    /* port 必须提供初始化和发送两个最小接口 */
    driver = fct_port_get_driver();
    if (driver == NULL || driver->init == NULL || driver->send == NULL)
    {
        return -2;
    }

    /* 硬件初始化失败时不保留半成品状态 */
    if (driver->init() != 0)
    {
        return -3;
    }

    g_driver      = driver;
    g_seq         = 0;
    g_state       = FCT_STATE_IDLE;
    g_initialized = true;

    return 0;
}

/**
 * @brief  反初始化 FCT 模块
 *
 * @return 0: 成功
 */
int fct_deinit(void)
{
    /* 未初始化时直接返回 */
    if (!g_initialized)
    {
        return 0;
    }

    if (g_driver != NULL && g_driver->deinit != NULL)
    {
        g_driver->deinit();
    }

    g_driver      = NULL;
    g_seq         = 0;
    g_state       = FCT_STATE_IDLE;
    g_initialized = false;

    return 0;
}

/**
 * @brief  开始一轮测试，发送 START
 *
 * @return 0: 成功, -1: 模块未初始化, -3: 已有测试进行中, -4: 发送失败
 */
int fct_start(void)
{
    int ret;

    if (!g_initialized)
    {
        return -1;
    }

    if (g_state != FCT_STATE_IDLE)
    {
        return -3;
    }

    /* START 自身使用 seq=0，并重置本轮序号 */
    g_seq = 0;
    ret = fct_send_frame("START", NULL);
    if (ret != 0)
    {
        return ret;
    }

    g_state = FCT_STATE_TESTING;
    return 0;
}

/**
 * @brief  发送一条 LOG
 *
 * @param  level 日志级别
 * @param  text  日志内容
 * @return 0: 成功, -1: 模块未初始化, -2: 参数错误, -3: 状态不允许, -4: 发送失败
 */
int fct_log(fct_log_level_t level, const char *text)
{
    char clean[FCT_LOG_TEXT_MAX + 1];
    char payload[FCT_LOG_TEXT_MAX + 16];

    if (!g_initialized)
    {
        return -1;
    }

    if (text == NULL || level < FCT_LOG_INFO || level > FCT_LOG_DEBUG)
    {
        return -2;
    }

    /* LOG 只应出现在 START 与 RESULT 之间 */
    if (g_state != FCT_STATE_TESTING)
    {
        return -3;
    }

    fct_clean_text(clean, text);
    snprintf(payload, sizeof(payload), "%s,%s", g_log_level_name[level], clean);

    return fct_send_frame("LOG", payload);
}

/**
 * @brief  按 printf 格式发送一条 LOG
 *
 * @param  level 日志级别
 * @param  fmt   格式字符串
 * @return 0: 成功, -1: 模块未初始化, -2: 参数错误, -3: 状态不允许, -4: 发送失败
 */
int fct_logf(fct_log_level_t level, const char *fmt, ...)
{
    char    text[FCT_LOG_TEXT_MAX + 1];
    va_list args;

    if (!g_initialized)
    {
        return -1;
    }

    if (fmt == NULL || level < FCT_LOG_INFO || level > FCT_LOG_DEBUG)
    {
        return -2;
    }

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    return fct_log(level, text);
}

/**
 * @brief  发送 RESULT，表示本轮测试的最终结论
 *
 * @param  result     FCT_RESULT_OK 或 FCT_RESULT_NG
 * @param  error_code 错误码，OK 必须为 0，NG 必须非 0
 * @return 0: 成功, -1: 模块未初始化, -2: 参数错误或结果与错误码矛盾,
 *         -3: 状态不允许, -4: 发送失败
 */
int fct_result(fct_result_t result, uint16_t error_code)
{
    char payload[16];
    int  ret;

    if (!g_initialized)
    {
        return -1;
    }

    if (result != FCT_RESULT_OK && result != FCT_RESULT_NG)
    {
        return -2;
    }

    /* 协议要求 OK 对应 0、NG 对应非 0，矛盾组合直接拒绝，避免上位机记录错乱 */
    if ((result == FCT_RESULT_OK && error_code != FCT_ERROR_CODE_NONE) ||
        (result == FCT_RESULT_NG && error_code == FCT_ERROR_CODE_NONE))
    {
        return -2;
    }

    /* 一轮测试只允许一条 RESULT */
    if (g_state != FCT_STATE_TESTING)
    {
        return -3;
    }

    snprintf(payload, sizeof(payload), "%s,%u",
             (result == FCT_RESULT_OK) ? "OK" : "NG",
             (unsigned int)error_code);

    ret = fct_send_frame("RESULT", payload);
    if (ret != 0)
    {
        return ret;
    }

    g_state = FCT_STATE_RESULT_SENT;
    return 0;
}

/**
 * @brief  发送 END，结束本轮测试
 *
 * @return 0: 成功, -1: 模块未初始化, -3: 没有进行中的测试, -4: 发送失败
 * @note   允许未发送 RESULT 直接结束，用于中止本轮。
 */
int fct_end(void)
{
    int ret;

    if (!g_initialized)
    {
        return -1;
    }

    if (g_state == FCT_STATE_IDLE)
    {
        return -3;
    }

    ret = fct_send_frame("END", NULL);
    if (ret != 0)
    {
        return ret;
    }

    g_state = FCT_STATE_IDLE;
    return 0;
}

/**
 * @brief  发送一条心跳 HB
 *
 * @return 0: 成功, -1: 模块未初始化, -4: 发送失败
 * @note   START 与 END 之间（含 RESULT 之后）上报 TESTING，其余上报 IDLE。
 */
int fct_hb(void)
{
    if (!g_initialized)
    {
        return -1;
    }

    return fct_send_frame("HB", (g_state == FCT_STATE_IDLE) ? "IDLE" : "TESTING");
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  组帧并发送一条报文，成功后 seq 加一
 *
 * @param  type    报文类型，START / LOG / RESULT / END / HB
 * @param  payload 附加字段，可为 NULL；允许包含逗号
 * @return 0: 成功, -1: 未初始化, -4: 组帧超长或发送失败
 * @note   发送失败不推进 seq、不改变测试状态，保证上位机看到的序号连续。
 */
static int fct_send_frame(const char *type, const char *payload)
{
    int len;

    if (!g_initialized || g_driver == NULL || g_driver->send == NULL)
    {
        return -1;
    }

    if (payload != NULL)
    {
        len = snprintf((char *)g_tx_buf, FCT_TX_BUF_SIZE,
                       "#FCT,1,%u,%lu,%s,%s\r\n",
                       (unsigned int)FCT_MODEL_TYPE,
                       (unsigned long)g_seq, type, payload);
    }
    else
    {
        len = snprintf((char *)g_tx_buf, FCT_TX_BUF_SIZE,
                       "#FCT,1,%u,%lu,%s\r\n",
                       (unsigned int)FCT_MODEL_TYPE,
                       (unsigned long)g_seq, type);
    }

    if (len <= 0 || len >= (int)FCT_TX_BUF_SIZE)
    {
        return -4;
    }

    if (g_driver->send(g_tx_buf, (uint32_t)len) != 0)
    {
        return -4;
    }

    g_seq++;
    return 0;
}

/**
 * @brief  把日志文本清洗成协议允许的 7-bit 可打印 ASCII
 *
 * @param  dst 输出缓冲区，至少 FCT_LOG_TEXT_MAX + 1 字节
 * @param  src 输入文本
 * @note   控制字符、CR/LF 和非 ASCII 字节（含中文）逐个替换为 '?'，
 *         超长部分截断，保证不会产生非法帧或断行。
 */
static void fct_clean_text(char *dst, const char *src)
{
    uint32_t i;

    for (i = 0; i < (uint32_t)FCT_LOG_TEXT_MAX && src[i] != '\0'; i++)
    {
        uint8_t ch = (uint8_t)src[i];

        dst[i] = (ch < 0x20U || ch > 0x7EU) ? '?' : src[i];
    }

    dst[i] = '\0';
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
