#ifndef __FCT_H_
#define __FCT_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>

#include "fct_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#define FCT_ERROR_CODE_NONE 0U      /* 成功固定使用的错误码 */

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  LOG 级别，对应协议 LOG 报文的级别字段
 */
typedef enum
{
    FCT_LOG_INFO = 0,   /* 一般信息 */
    FCT_LOG_WARN,       /* 警告 */
    FCT_LOG_ERROR,      /* 错误 */
    FCT_LOG_DEBUG,      /* 调试 */
} fct_log_level_t;

/**
 * @brief  测试结果，对应协议 RESULT 报文
 */
typedef enum
{
    FCT_RESULT_OK = 0,  /* 通过 */
    FCT_RESULT_NG,      /* 不通过 */
} fct_result_t;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/*
 * 返回值约定：
 *    0: 成功
 *   -1: 模块未初始化
 *   -2: 参数错误
 *   -3: 当前状态不允许该报文
 *   -4: 发送失败或组帧超长
 *
 * 模块为单实例，内部不加锁；多个任务并发调用会导致 seq 重复，
 * 调用方需保证所有 fct_xxx() 在同一上下文执行。
 */

/**
 * @brief  初始化 FCT 模块
 *
 * 内部调用 fct_port_get_driver() 获取平台串口驱动并完成硬件初始化。
 * 型号由 fct_config.h 的 FCT_MODEL_TYPE 决定。
 *
 * @return 0: 成功, -1: 配置无效, -2: port 驱动无效, -3: 串口初始化失败
 * @note   模块为单实例，重复调用会先反初始化。
 */
int fct_init(void);

/**
 * @brief  反初始化 FCT 模块
 *
 * @return 0: 成功
 */
int fct_deinit(void);

/**
 * @brief  开始一轮测试，发送 START
 *
 * START 自身使用 seq=0 并重置本轮序号，之后每条报文 seq 加一。
 *
 * @return 0: 成功, -1: 模块未初始化, -3: 已有测试进行中, -4: 发送失败
 */
int fct_start(void);

/**
 * @brief  发送一条 LOG
 *
 * @param  level 日志级别
 * @param  text  日志内容，只读，不会被修改
 * @return 0: 成功, -1: 模块未初始化, -2: 参数错误, -3: 不在测试中或 RESULT 已发送, -4: 发送失败
 * @note   非可打印 ASCII 字节（含中文、CR/LF）逐个替换为 '?'，
 *         超过 FCT_LOG_TEXT_MAX 的部分截断；半角逗号属于可打印字符，保留。
 */
int fct_log(fct_log_level_t level, const char *text);

/**
 * @brief  按 printf 格式发送一条 LOG
 *
 * @param  level 日志级别
 * @param  fmt   格式字符串，不能为 NULL
 * @param  ...   格式参数
 * @return 0: 成功, -1: 模块未初始化, -2: 参数错误, -3: 不在测试中或 RESULT 已发送, -4: 发送失败
 * @note   格式化结果超过 FCT_LOG_TEXT_MAX 时截断，其余文本处理同 fct_log()。
 */
int fct_logf(fct_log_level_t level, const char *fmt, ...);

/**
 * @brief  发送 RESULT，表示本轮测试的最终结论
 *
 * @param  result     FCT_RESULT_OK 或 FCT_RESULT_NG
 * @param  error_code 错误码，十进制 uint16；OK 必须为 FCT_ERROR_CODE_NONE，
 *                    NG 必须非 0，具体含义按 fctType 由现场定义
 * @return 0: 成功, -1: 模块未初始化, -2: 参数错误或结果与错误码矛盾,
 *         -3: 不在测试中或 RESULT 已发送, -4: 发送失败
 * @note   一轮测试只应发送一条 RESULT；失败细节用 LOG 表达。
 */
int fct_result(fct_result_t result, uint16_t error_code);

/**
 * @brief  发送 END，结束本轮测试
 *
 * @return 0: 成功, -1: 模块未初始化, -3: 没有进行中的测试, -4: 发送失败
 * @note   允许在未发送 RESULT 的情况下结束，用于中止本轮。
 */
int fct_end(void);

/**
 * @brief  发送一条心跳 HB
 *
 * @return 0: 成功, -1: 模块未初始化, -4: 发送失败
 * @note   IDLE / TESTING 由模块当前状态自动选择；心跳周期由调用方控制，
 *         通常每 2 秒调用一次，模块内部不做计时。
 */
int fct_hb(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
