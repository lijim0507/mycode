#ifndef __UDS_H_
#define __UDS_H_

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

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef enum
{
    UDS_SESSION_DEFAULT     = 0x01,
    UDS_SESSION_PROGRAMMING = 0x02,
    UDS_SESSION_EXTENDED    = 0x03,
} uds_session_t;

typedef enum
{
    UDS_SEC_LEVEL_LOCKED = 0x00,
    UDS_SEC_LEVEL_1      = 0x01,
    UDS_SEC_LEVEL_2      = 0x02,
} uds_security_level_t;

typedef struct
{
    uint32_t request_id;
    uint32_t response_id;
} uds_port_config_t;

/****************************************************************************/
/*						Exported Functions								*/
/****************************************************************************/

/**
 * @brief 初始化 UDS 层，包括 ISO-TP 初始化、句柄配置和回调注册
 *
 * @return int 0 表示成功，负值表示失败
 */
int  uds_init(void);

/**
 * @brief 反初始化 UDS 层，释放 ISO-TP 资源
 */
void uds_deinit(void);

/**
 * @brief 轮询 ISO-TP，处理多帧续传、超时和 CAN 接收
 *
 * 需在主循环中周期性调用。内部通过 port driver 的
 * receive() 自动从环形缓冲区取出 CAN 帧。
 */
void uds_poll(void);

/**
 * @brief 处理 UDS 请求，在收到完整 ISO-TP 数据后由主循环调用
 *
 * 检查是否有请求标志，若有则解析 SID、查 Commands_Table、
 * 调用对应 handler，并发送响应。
 */
void uds_process(void);

/**
 * @brief 通过 ISO-TP 发送 UDS 响应数据
 *
 * @param data 数据指针
 * @param len  数据长度
 */
void uds_send_response(const uint8_t *data, uint16_t len);

/**
 * @brief 发送 UDS 负响应 (Negative Response)
 *
 * @param sid      服务 ID
 * @param nrc_code NRC 错误码
 */
void uds_send_negative_response(uint8_t sid, uint8_t nrc_code);

/**
 * @brief 获取当前诊断会话状态
 *
 * @return uds_session_t 当前会话
 */
uds_session_t        uds_get_session(void);

/**
 * @brief 获取当前安全访问等级
 *
 * @return uds_security_level_t 当前安全等级
 */
uds_security_level_t uds_get_security_level(void);

/**
 * @brief 设置诊断会话状态
 *
 * @param session 目标会话
 */
void                 uds_set_session(uds_session_t session);

/**
 * @brief 设置安全访问等级
 *
 * @param level 目标安全等级
 */
void                 uds_set_security_level(uds_security_level_t level);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/