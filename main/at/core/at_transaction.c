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
#define AT_FRAME_HEADER_LEN    2                                /* "AT" 长度                     */
#define AT_FRAME_TAIL_LEN      2                                /* "\r\n" 长度                   */
#define AT_FRAME_HEADER        "AT"                             /* AT 帧头                       */
#define AT_FRAME_TAIL          "\r\n"                           /* AT 帧尾                       */
#define AT_FRAME_BUF_SIZE      (AT_FRAME_HEADER_LEN + AT_CMD_MAX_LEN + AT_FRAME_TAIL_LEN)
#define AT_INTER_FRAME_MS      200                              /* 帧间超时(ms)                  */
#define AT_RECV_POLL_MS        10                               /* 阻塞等待时的轮询间隔(ms)        */
#define AT_LINE_BUF_SIZE       256                              /* 单行最大长度                   */

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/


/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

uint8_t g_transaction_flag = 0;
at_result_t g_transaction_result;

uint8_t g_resp_line_buf[AT_RESP_MAX_LINES][AT_RESP_BUF_SIZE];
uint16_t g_resp_line_len[AT_RESP_MAX_LINES];
uint16_t g_resp_line_num = 0;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/
void at_transaction_send(uint8_t * p_data, uint16_t len)
{
	if (!g_at_driver || !g_at_driver->send || !p_data || len == 0)
	{
		return -1;
	}

	if (len > AT_CMD_MAX_LEN)
	{
		return -1;
	}

	uint8_t frame_buf[AT_FRAME_BUF_SIZE];
	uint32_t frame_len = 0;

    g_transaction_flag = 1;


	memcpy(frame_buf, AT_FRAME_HEADER, AT_FRAME_HEADER_LEN);
	frame_len += AT_FRAME_HEADER_LEN;

	memcpy(frame_buf + frame_len, p_data, len);
	frame_len += len;

	memcpy(frame_buf + frame_len, AT_FRAME_TAIL, AT_FRAME_TAIL_LEN);
	frame_len += AT_FRAME_TAIL_LEN;

	return g_at_driver->send(frame_buf, frame_len);

}


uint8_t at_transaction_get_flag(void)
{
    return g_transaction_flag;
}

/*
 * at_transaction_end_marker_check: 判断当前行是否为响应结束语。
 * 返回值: true=结束语, out_status 输出对应的状态。
 */
bool at_transaction_end_marker_check(const char *line)
{
	if (strcmp(line, "OK") == 0)
	{
		g_transaction_result = AT_RESULT_OK;
		return true;
	}

	if (strcmp(line, "ERROR") == 0)
	{
		g_transaction_result = AT_RESULT_ERROR;
		return true;
	}
    
	if (strncmp(line, "+CME ERROR:", 11) == 0)
	{
		g_transaction_result = AT_RESULT_ERROR;
		return true;
	}

	if (strncmp(line, "+CMS ERROR:", 11) == 0)
	{
		g_transaction_result = AT_RESULT_ERROR;
		return true;
	}

	if (strncmp(line, ">", 1) == 0)
	{
		g_transaction_result = AT_RESULT_ENTER_PAYLOAD;
		return true;
	}

	return false;
}

bool at_transaction_get_respond(uint8_t *p_buffer, uint16_t len)
{
    if (g_resp_line_num >= AT_RESP_MAX_LINES)
    {
        return false;
    }

    memcpy(g_resp_line_buf[g_resp_line_num], p_buffer, len);
    g_resp_line_len[g_resp_line_num] = len;
    g_resp_line_num++;

    return true;
}


void at_transaction_reset(void)
{
    g_transaction_flag = 0;

    for (int i = 0; i < AT_RESP_MAX_LINES; i++)
    {
        memset(g_resp_line_buf[i], 0, AT_RESP_BUF_SIZE);
        g_resp_line_len[i] = 0;
    }
    g_resp_line_num = 0;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/


/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
