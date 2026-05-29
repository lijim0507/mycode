/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "at.h"
#include "at_port.h"

#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define AT_INTER_FRAME_MS    200                                /* 帧间超时(ms)                */
#define AT_RECV_STEP_MS      50                                 /* 非阻塞轮询步长              */
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

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
const at_uart_driver_t *g_at_driver;
static bool              g_initialized;
static at_active_t       g_active;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int at_init(const at_uart_driver_t *driver, void *port_cfg)
{
    if (!driver || !driver->init || !driver->send || !driver->recv)
    {
        return -1;
    }

    if (g_initialized)
    {
        at_deinit();
    }

    if (driver->init(port_cfg) != 0)
    {
        return -2;
    }

    g_at_driver   = driver;
    g_initialized = true;
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

    g_at_driver   = NULL;
    g_initialized = false;
    g_active.active = false;

    return 0;
}

int at_urc_register(const char *prefix, at_urc_handler_t handler, void *user_data)
{
    return at_parser_urc_register(prefix, handler, user_data);
}

int at_urc_unregister(const char *prefix)
{
    return at_parser_urc_unregister(prefix);
}

/* ======================== 阻塞 API ======================== */

int at_send_command(const char *cmd, at_resp_t *resp, uint32_t timeout_ms)
{
    return at_frame_send_cmd(cmd, resp, timeout_ms);
}

int at_send_command_data(const char *cmd, const uint8_t *data, uint32_t data_len,
                         at_resp_t *resp, uint32_t timeout_ms)
{
    if (!cmd || !resp)
    {
        return -1;
    }

    uint8_t  content[AT_CMD_MAX_LEN];
    uint32_t content_len = 0;

    {
        uint32_t cmd_len = strlen(cmd);
        if (cmd_len > AT_CMD_MAX_LEN)
        {
            return -1;
        }
        memcpy(content, cmd, cmd_len);
        content_len = cmd_len;
    }

    if (data != NULL && data_len > 0)
    {
        if (content_len + 1 + data_len > AT_CMD_MAX_LEN)
        {
            return -1;
        }
        content[content_len] = '=';
        content_len++;
        memcpy(content + content_len, data, data_len);
        content_len += data_len;
    }

    int ret = at_frame_send(content, content_len);
    if (ret != 0)
    {
        return ret;
    }

    return at_frame_recv(resp, timeout_ms);
}

/* ======================== 非阻塞 API ======================== */

int at_send_async(const char *cmd, at_async_callback_t cb, void *user_data,
                  uint32_t timeout_ms)
{
    int ret;

    if (!cmd || !cb)
    {
        return -1;
    }

    if (g_active.active)
    {
        return -1;
    }

    ret = at_frame_send((const uint8_t *)cmd, strlen(cmd));
    if (ret != 0)
    {
        return -2;
    }

    g_active.active     = true;
    g_active.callback   = cb;
    g_active.user_data  = user_data;
    g_active.timeout_ms = timeout_ms;
    g_active.send_tick  = pdTICKS_TO_MS(xTaskGetTickCount());

    return 0;
}

int at_send_data_async(const char *cmd, const uint8_t *data, uint32_t data_len,
                       at_async_callback_t cb, void *user_data, uint32_t timeout_ms)
{
    int ret;

    if (!cmd || !cb)
    {
        return -1;
    }

    if (g_active.active)
    {
        return -1;
    }

    uint8_t  content[AT_CMD_MAX_LEN];
    uint32_t content_len = 0;

    {
        uint32_t cmd_len = strlen(cmd);
        if (cmd_len > AT_CMD_MAX_LEN)
        {
            return -1;
        }
        memcpy(content, cmd, cmd_len);
        content_len = cmd_len;
    }

    if (data != NULL && data_len > 0)
    {
        if (content_len + 1 + data_len > AT_CMD_MAX_LEN)
        {
            return -1;
        }
        content[content_len] = '=';
        content_len++;
        memcpy(content + content_len, data, data_len);
        content_len += data_len;
    }

    ret = at_frame_send(content, content_len);
    if (ret != 0)
    {
        return -2;
    }

    g_active.active     = true;
    g_active.callback   = cb;
    g_active.user_data  = user_data;
    g_active.timeout_ms = timeout_ms;
    g_active.send_tick  = pdTICKS_TO_MS(xTaskGetTickCount());

    return 0;
}

/*
 * 非阻塞轮询。
 * 两阶段驱动：
 * 1) 读 UART → feed 积累器，检查帧格式完整性
 * 2) 帧完成 → get_frame → analyze → 回调分发
 * 3) 无数据时检查帧间超时 → 触发帧完成
 * 4) 检查活跃命令的超时
 */
int at_recv_poll(void)
{
    uint8_t  recv_buf[256];
    int      len;
    uint64_t now;
    char     frame_buf[AT_RESP_BUF_SIZE];
    uint32_t frame_len;
    char     resp_data[AT_RESP_BUF_SIZE];
    uint32_t resp_len;

    if (!g_at_driver)
    {
        return -1;
    }

    now = pdTICKS_TO_MS(xTaskGetTickCount());

    /* 检查活跃命令的整体超时 */
    if (g_active.active)
    {
        if (now - g_active.send_tick > g_active.timeout_ms)
        {
            at_parser_get_frame(frame_buf, &frame_len);

            at_async_callback_t cb = g_active.callback;
            void *ud = g_active.user_data;
            g_active.active = false;

            if (cb)
            {
                if (frame_len > 0 && frame_len < AT_RESP_BUF_SIZE)
                {
                    memcpy(resp_data, frame_buf, frame_len);
                }
                else
                {
                    frame_len = 0;
                }
                cb(AT_RESP_TIMEOUT, resp_data, frame_len, ud);
            }
            return 1;
        }
    }

    /* 非阻塞读取 UART → 阶段一：积累 */
    len = g_at_driver->recv(recv_buf, sizeof(recv_buf), 0);
    if (len > 0)
    {
        int complete = at_parser_feed((const char *)recv_buf, (uint32_t)len);
        if (complete)
        {
            /* 阶段二：帧格式完成，分析 */
            at_parser_get_frame(frame_buf, &frame_len);
            at_resp_status_t status = at_parser_analyze(frame_buf, frame_len,
                                                         resp_data, &resp_len);

            /* 若为命令响应则回调 */
            if ((status == AT_RESP_OK || status == AT_RESP_ERROR)
                && g_active.active)
            {
                at_async_callback_t cb = g_active.callback;
                void *ud = g_active.user_data;
                g_active.active = false;

                if (cb)
                {
                    cb(status, resp_data, resp_len, ud);
                }
            }
        }
        return 1;
    }

    /* 无新数据 → 检查帧间超时 */
    if (at_parser_check_timeout(AT_INTER_FRAME_MS))
    {
        at_parser_get_frame(frame_buf, &frame_len);
        at_resp_status_t status = at_parser_analyze(frame_buf, frame_len,
                                                     resp_data, &resp_len);

        /* 若为命令响应则回调 */
        if ((status == AT_RESP_OK || status == AT_RESP_ERROR)
            && g_active.active)
        {
            at_async_callback_t cb = g_active.callback;
            void *ud = g_active.user_data;
            g_active.active = false;

            if (cb)
            {
                cb(status, resp_data, resp_len, ud);
            }
        }
        return 1;
    }

    return 0;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
