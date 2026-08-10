/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "at.h"
#include "at_port.h"

#include <string.h>
#include <stdbool.h>
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define AT_INTER_FRAME_MS    200                                /* 帧间超时(ms)                */
#define AT_RECV_BUF_SIZE     512                                /* 单次接收缓冲                 */
/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef struct
{
    bool                 active;
    at_async_callback_t  callback;
    void                *user_data;
    uint32_t             timeout_ms;
    uint64_t             send_tick;
} at_active_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

void at_transaction_send(uint8_t * p_data, uint16_t len);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
const uart_port_driver_t *g_at_driver;
static bool                 g_initialized;
static at_active_t          g_active;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 AT 模块
 *
 * 内部调用 at_port_get_driver() 获取平台 UART 驱动，
 * port 负责配置 UART 硬件并通过 receive 提供 poll 接收。
 *
 * @return 0: 成功, -1: driver 无效, -2: 硬件初始化失败
 */
int at_init(void)
{
    const uart_port_driver_t *driver;

    driver = at_port_get_driver();
    if (!driver || !driver->init || !driver->send || !driver->receive)
    {
        return -1;
    }

    if (g_initialized)
    {
        at_deinit();
    }

    if (driver->init() != 0)
    {
        return -2;
    }

    g_at_driver    = driver;
    g_initialized  = true;
    g_active.active = false;

    return 0;
}

int at_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    if (g_at_driver && g_at_driver->deinit)
    {
        g_at_driver->deinit();
    }

    g_at_driver    = NULL;
    g_initialized  = false;
    g_active.active = false;

    return 0;
}

/**
 * @brief 发送 AT 命令并等待响应（poll 模式）
 *
 * 1. 发送命令
 * 2. 轮询 g_at_driver->receive() 读取 UART 数据
 * 3. 将收到的数据送入 at_parser_recv_data() 解析
 * 4. 检查事务完成或超时
 *
 * @param cmd         AT 命令字符串
 * @param resp        响应结构体（输出）
 * @param timeout_ms  超时时间（ms）
 * @return at_result_t
 */
at_result_t at_exec(const char *cmd, at_response_t *resp, uint32_t timeout_ms)
{
    uint8_t recv_buf[AT_RECV_BUF_SIZE];
    uint32_t recv_len;
    uint32_t elapsed;
    int      ret;

    /* 发送命令 */
    at_transaction_send((uint8_t *)cmd, (uint16_t)strlen(cmd));

    /* 轮询接收直到超时或响应完成 */
    elapsed = 0;
    while (elapsed < timeout_ms)
    {
        recv_len = AT_RECV_BUF_SIZE;
        ret = g_at_driver->receive(recv_buf, &recv_len, AT_INTER_FRAME_MS);

        if (ret > 0 && recv_len > 0)
        {
            at_frame_type_t frame_type;

            /* 喂入解析器 */
            for (uint32_t i = 0; i < recv_len; i++)
            {
                frame_type = at_parser_recv_data(recv_buf[i]);
                if (frame_type == AT_FRAME_COMPLETE_RESP ||
                    frame_type == AT_FRAME_COMPLETE_URC)
                {
                    return (frame_type == AT_FRAME_COMPLETE_RESP)
                           ? AT_RESULT_OK : AT_RESULT_ERROR;
                }
            }
        }

        elapsed += AT_INTER_FRAME_MS;
    }

    return AT_RESULT_TIMEOUT;
}

/**
 * @brief 发送 AT 命令（带预期内容），等待响应
 *
 * @param cmd         AT 命令字符串
 * @param content     附加数据内容（如 SMS 正文）
 * @param expect      期待的响应关键字
 * @param resp        响应结构体（输出）
 * @param timeout_ms  超时时间（ms）
 * @return at_result_t
 */
at_result_t at_exec_ex(const char *cmd, const char *content,
                        const char *expect, at_response_t *resp,
                        uint32_t timeout_ms)
{
    uint8_t recv_buf[AT_RECV_BUF_SIZE];
    uint32_t recv_len;
    uint32_t elapsed;
    int      ret;

    /* 发送命令 */
    at_transaction_send((uint8_t *)cmd, (uint16_t)strlen(cmd));

    /* 如有附加内容，继续发送（如 SMS 正文以 Ctrl-Z 结束） */
    if (content)
    {
        at_transaction_send((uint8_t *)content, (uint16_t)strlen(content));
    }

    /* 轮询接收 */
    elapsed = 0;
    while (elapsed < timeout_ms)
    {
        recv_len = AT_RECV_BUF_SIZE;
        ret = g_at_driver->receive(recv_buf, &recv_len, AT_INTER_FRAME_MS);

        if (ret > 0 && recv_len > 0)
        {
            at_frame_type_t frame_type;

            for (uint32_t i = 0; i < recv_len; i++)
            {
                frame_type = at_parser_recv_data(recv_buf[i]);
                if (frame_type == AT_FRAME_COMPLETE_RESP)
                {
                    return AT_RESULT_OK;
                }
                else if (frame_type == AT_FRAME_COMPLETE_URC)
                {
                    return AT_RESULT_ERROR;
                }
            }
        }

        elapsed += AT_INTER_FRAME_MS;
    }

    return AT_RESULT_TIMEOUT;
}

int at_register_urc(const char *prefix, at_urc_handler_t handler, void *user_data)
{
    return at_parser_urc_register(prefix, handler, user_data);
}

int at_unregister_urc(const char *prefix)
{
    return at_parser_urc_unregister(prefix);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
