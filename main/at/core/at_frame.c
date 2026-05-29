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
#define AT_FRAME_HEADER_LEN  2                                  /* "AT" 长度         */
#define AT_FRAME_TAIL_LEN    2                                  /* "\r\n" 长度       */
#define AT_FRAME_HEADER      "AT"                               /* AT 帧头           */
#define AT_FRAME_TAIL        "\r\n"                             /* AT 帧尾           */
#define AT_FRAME_BUF_SIZE    (AT_FRAME_HEADER_LEN + AT_CMD_MAX_LEN + AT_FRAME_TAIL_LEN)
#define AT_RECV_STEP_MS      50                                 /* 每次轮询间隔(ms)   */
#define AT_INTER_FRAME_MS    200                                /* 帧间超时(ms)      */
/****************************************************************************/
/*                              Typedefs                                    */
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

/*
 * 拼装并发送 AT 帧：AT<content>\r\n
 */
int at_frame_send(const uint8_t *content, uint32_t content_len)
{
    int      ret;
    uint8_t  frame_buf[AT_FRAME_BUF_SIZE];
    uint32_t frame_len = 0;

    if (!g_at_driver)
    {
        return -1;
    }

    memcpy(frame_buf, AT_FRAME_HEADER, AT_FRAME_HEADER_LEN);
    frame_len += AT_FRAME_HEADER_LEN;

    if (content != NULL && content_len > 0)
    {
        if (frame_len + content_len > AT_FRAME_BUF_SIZE)
        {
            content_len = AT_FRAME_BUF_SIZE - frame_len;
        }
        memcpy(frame_buf + frame_len, content, content_len);
        frame_len += content_len;
    }

    memcpy(frame_buf + frame_len, AT_FRAME_TAIL, AT_FRAME_TAIL_LEN);
    frame_len += AT_FRAME_TAIL_LEN;

    ret = g_at_driver->send(frame_buf, frame_len);
    if (ret != 0)
    {
        return -2;
    }

    return 0;
}

/*
 * 阻塞接收 AT 响应。
 * 两阶段：先积累完整帧(流式 + 格式匹配/超时)，再统一分析帧内容。
 * URC 在分析阶段自动分发，命令响应数据写入 resp。
 * 返回值: 0=成功, -1=参数无效, -3=超时
 */
int at_frame_recv(at_resp_t *resp, uint32_t timeout_ms)
{
    uint64_t start_tick;
    uint8_t  recv_buf[256];
    int      len;
    char     frame_buf[AT_RESP_BUF_SIZE];
    uint32_t frame_len;

    if (!g_at_driver || !resp)
    {
        return -1;
    }

    memset(resp, 0, sizeof(*resp));
    start_tick = pdTICKS_TO_MS(xTaskGetTickCount());

    while (pdTICKS_TO_MS(xTaskGetTickCount()) - start_tick < timeout_ms)
    {
        len = g_at_driver->recv(recv_buf, sizeof(recv_buf), AT_RECV_STEP_MS);
        if (len > 0)
        {
            /* 阶段一：积累数据，检查帧格式完整性 */
            int complete = at_parser_feed((const char *)recv_buf, (uint32_t)len);
            if (complete)
            {
                /* 阶段二：取出完整帧，分析帧内容 */
                at_parser_get_frame(frame_buf, &frame_len);
                resp->status = at_parser_analyze(frame_buf, frame_len,
                                                  resp->data, &resp->data_len);
                return 0;
            }
        }
        else
        {
            /* 无新数据，检查帧间超时（URC 等无结束标志的帧） */
            if (at_parser_check_timeout(AT_INTER_FRAME_MS))
            {
                at_parser_get_frame(frame_buf, &frame_len);
                resp->status = at_parser_analyze(frame_buf, frame_len,
                                                  resp->data, &resp->data_len);
                return 0;
            }
        }
    }

    /* 整体超时，取出已积累数据作为响应 */
    at_parser_get_frame(frame_buf, &frame_len);
    resp->status = AT_RESP_TIMEOUT;
    if (frame_len < AT_RESP_BUF_SIZE)
    {
        memcpy(resp->data, frame_buf, frame_len);
        resp->data_len = frame_len;
    }
    return -3;
}

/*
 * 发送 cmd 字符串帧并等待响应
 */
int at_frame_send_cmd(const char *cmd, at_resp_t *resp, uint32_t timeout_ms)
{
    int ret;

    if (!cmd || !resp)
    {
        return -1;
    }

    ret = at_frame_send((const uint8_t *)cmd, strlen(cmd));
    if (ret != 0)
    {
        return ret;
    }

    return at_frame_recv(resp, timeout_ms);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
