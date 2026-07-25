/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds.h"
#include "uds_defs.h"
#include "uds_service.h"
#include "isotp.h"
#include "uds_port.h"

#include <string.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void uds_on_recv(uint8_t *data, uint16_t len);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static isotp_handle_t g_isotp_handle;

static volatile uint8_t  g_request_flag;
static uint16_t           g_request_len;
static uint8_t            g_request_buf[UDS_BUF_SIZE];

static uds_request_t   g_request;
static uds_response_t  g_response;

static uint8_t         g_initialized;

static uds_session_t        g_current_session        = UDS_SESSION_DEFAULT;
static uds_security_level_t g_current_security_level = UDS_SEC_LEVEL_LOCKED;


/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief ISO-TP 接收回调，收到完整数据后置标志位，由主循环 uds_process() 处理
 *
 * @param data 数据指针
 * @param len  数据长度
 */
static void uds_on_recv(uint8_t *data, uint16_t len)
{
    if (len > UDS_BUF_SIZE)
    {
        return;
    }

    (void)memcpy(g_request_buf, data, len);
    g_request_len = len;
    g_request_flag = 1;
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief 初始化 UDS 层，包括 ISO-TP 初始化、句柄配置和回调注册
 *
 * @return int 0 表示成功，负值表示失败
 */
int uds_init(void)
{
    const uds_port_config_t *config;

    if (g_initialized)
    {
        uds_deinit();
    }

    config = uds_port_get_config();
    if (config == NULL)
    {
        return -1;
    }

    if (isotp_init() != ISOTP_RET_OK)
    {
        return -2;
    }

    isotp_init_handle(&g_isotp_handle,  config->request_id, config->response_id);
    isotp_register_recv_cb(&g_isotp_handle, uds_on_recv);

    g_current_session        = UDS_SESSION_DEFAULT;
    g_current_security_level = UDS_SEC_LEVEL_LOCKED;
    g_request_flag            = 0;
    g_request_len             = 0;

    g_initialized = 1;
    return 0;
}

/**
 * @brief 反初始化 UDS 层，释放 ISO-TP 资源
 */
void uds_deinit(void)
{
    if (!g_initialized)
    {
        return;
    }

    isotp_deinit();

    g_initialized = 0;
}

/**
 * @brief 轮询 ISO-TP，处理多帧续传、超时和 CAN 接收
 *
 * 需在主循环中周期性调用。内部通过 port driver 的
 * receive() 自动从环形缓冲区取出 CAN 帧。
 */
void uds_poll(void)
{
    if (!g_initialized)
    {
        return;
    }
    isotp_poll(&g_isotp_handle);
}

/**
 * @brief 处理 UDS 请求
 *
 * 检查是否有请求标志，若有则解析 SID、查 Commands_Table、
 * 调用对应 handler，并发送响应。
 */
void uds_process(void)
{
    uint8_t sid;
    uds_service_type_t    expected_type;
    uds_service_handler_t handler;
    uds_handler_result_t  result;

    if (!g_request_flag)
    {
        return;
    }

    g_request_flag = 0;

    if (g_request_len == 0)
    {
        return;
    }

    sid = g_request_buf[0];

    /*查询是否支持服务 */
    if (sid >= UDS_SERVICE_ID_RESERVED_MAX)
    {
        uds_send_negative_response(sid, NRC_SERVICE_NOT_SUPPORTED);
        return;
    }

    /* 查询服务类型和处理函数 */
    expected_type = services_table[sid].service_type;
    handler       = services_table[sid].handler;

    if (expected_type == UDS_SERVICE_TYPE_NONE || handler == NULL)
    {
        uds_send_negative_response(sid, NRC_SERVICE_NOT_SUPPORTED);
        return;
    }

    (void)memset(&g_request,  0, sizeof(g_request));
    (void)memset(&g_response, 0, sizeof(g_response));

    switch (expected_type)
    {
        case UDS_SERVICE_TYPE_SID_ONLY:
        {
            g_request.sid      = g_request_buf[0];
            g_request.sub_sid  = 0;
            g_request.data_len = 0;
            break;
        }
        case UDS_SERVICE_TYPE_SID_SUB:
        {
            if (g_request_len < 2)
            {
                uds_send_negative_response(sid, NRC_INCORRECT_MESSAGE_LENGTH);
                return;
            }
            g_request.sid      = g_request_buf[0];
            g_request.sub_sid  = g_request_buf[1];
            g_request.data_len = 0;
            break;
        }
        case UDS_SERVICE_TYPE_SID_DATA:
        {
            if (g_request_len < 2)
            {
                uds_send_negative_response(sid, NRC_INCORRECT_MESSAGE_LENGTH);
                return;
            }
            g_request.sid      = g_request_buf[0];
            g_request.sub_sid  = 0;
            g_request.data_len = g_request_len - 1;
            if (g_request.data_len > UDS_BUF_SIZE)
            {
                g_request.data_len = UDS_BUF_SIZE;
            }
            (void)memcpy(g_request.data, &g_request_buf[1], g_request.data_len);
            break;
        }
        case UDS_SERVICE_TYPE_SID_SUB_DATA:
        {
            if (g_request_len < 3)
            {
                uds_send_negative_response(sid, NRC_INCORRECT_MESSAGE_LENGTH);
                return;
            }
            g_request.sid      = g_request_buf[0];
            g_request.sub_sid  = g_request_buf[1];
            g_request.data_len = g_request_len - 2;
            if (g_request.data_len > UDS_BUF_SIZE)
            {
                g_request.data_len = UDS_BUF_SIZE;
            }
            (void)memcpy(g_request.data, &g_request_buf[2], g_request.data_len);
            break;
        }
        default:
        {
            uds_send_negative_response(sid, NRC_SERVICE_NOT_SUPPORTED);
            return;
        }
    }

    result = handler(&g_request, &g_response);

    if (result == UDS_HANDLER_NRC_SENT)
    {
        return;
    }
    if (result == UDS_HANDLER_ERROR)
    {
        uds_send_negative_response(sid, NRC_GENERAL_REJECT);
        return;
    }

    {
        static uint8_t send_buf[UDS_BUF_SIZE];
        uint16_t send_len = 1 + g_response.data_len;

        send_buf[0] = g_response.sid;
        if (g_response.data_len > 0)
        {
            (void)memcpy(&send_buf[1], g_response.data, g_response.data_len);
        }

        uds_send_response(send_buf, send_len);
    }
}

/**
 * @brief 通过 ISO-TP 发送 UDS 响应数据
 *
 * @param data 数据指针
 * @param len  数据长度
 */
void uds_send_response(const uint8_t *data, uint16_t len)
{
    if (!g_initialized || data == NULL || len == 0)
    {
        return;
    }
    isotp_send(&g_isotp_handle, data, len);
}

/**
 * @brief 发送 UDS 负响应 (Negative Response)
 *
 * @param sid      服务 ID
 * @param nrc_code NRC 错误码
 */
void uds_send_negative_response(uint8_t sid, uint8_t nrc_code)
{
    uint8_t buf[3];
    buf[0] = UDS_NEGATIVE_RESPONSE_SID;
    buf[1] = sid;
    buf[2] = nrc_code;
    uds_send_response(buf, 3);
}

/**
 * @brief 获取当前诊断会话状态
 *
 * @return uds_session_t 当前会话
 */
uds_session_t uds_get_session(void)
{
    return g_current_session;
}

/**
 * @brief 获取当前安全访问等级
 *
 * @return uds_security_level_t 当前安全等级
 */
uds_security_level_t uds_get_security_level(void)
{
    return g_current_security_level;
}

/**
 * @brief 设置诊断会话状态
 *
 * @param session 目标会话
 */
void uds_set_session(uds_session_t session)
{
    g_current_session = session;
}

/**
 * @brief 设置安全访问等级
 *
 * @param level 目标安全等级
 */
void uds_set_security_level(uds_security_level_t level)
{
    g_current_security_level = level;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/