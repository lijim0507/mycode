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

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
bool at_transaction_end_marker_check(const char *line);
uint8_t at_transaction_get_flag(void);
bool at_transaction_get_respond(uint8_t *p_buffer, uint16_t len);


int at_urc_check_and_dispatch(const char *line);

static void at_parser_reset_buffer(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/* ── 行积累 ── */
static char     g_line_buf[AT_LINE_BUF_SIZE];                  /* 当前行积累缓冲                 */
static uint32_t g_line_len;                                    /* 当前行已积累字节数              */

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/


/*
 * at_parser_recv_data: 在 UART 中断/RX 回调上下文中调用。
 *
 * 逐字节积累，每当收到完整的行 (\n) 时：
 *   1. 检查是否匹配已注册的 URC 前缀 → 匹配则立即分发，不进入响应缓冲
 *   2. 检查是否为结束语 (OK / ERROR / +CME ERROR: / +CMS ERROR:) → 标记完成
 *   3. 检查是否为 > 提示符 → 填入响应缓冲，不结束帧
 *   4. 其余行 → 填入响应缓冲
 *
 * 返回值:
 *   AT_FRAME_INCOMPLETE     — 继续积累 (URC 已在本函数内分发)
 *   AT_FRAME_COMPLETE_RESP  — 响应帧完整 (遇到结束语)
 */
void at_parser_recv_data(const uint8_t *data, uint32_t len)
{
	if (!data || len == 0)
	{
		return AT_FRAME_INCOMPLETE;
	}

	g_rx_last_tick = pdTICKS_TO_MS(xTaskGetTickCount());

	for (uint32_t i = 0; i < len; i++)
	{
		char ch = (char)data[i];


		/* 一行结束: \n 分析是respond command还是urc */
		if (ch == '\r' && data[i + 1] == '\n')
		{
            i++;    //跳过 /n

			if (g_line_len == 0)
			{
				continue;
			}

            //将 g_line_buf 变成合法的 C 字符串，这样后续的 strcmp()、strncmp() 才能安全操作
			g_line_buf[g_line_len] = '\0'; 

			/* ① 匹配 URC 前缀 → 立即分发，不进响应缓冲 */
			if (at_urc_check_and_dispatch(g_line_buf))
			{
				g_line_len = 0;
				continue;
			}

			//判断是否处于事务中（已发送命令，正在等待响应），如果不是事务，则不进行结束语判断，直接丢弃该行数据
			if(at_transaction_get_flag())
			{
				/* ② 结束语判断 */
				if (at_transaction_end_marker_check(g_line_buf))
				{

				}
				else
				{

					at_transaction_get_respond((uint8_t *)g_line_buf, g_line_len);
					
				}
				g_line_len = 0;
				continue;
			}
		}

		/* 普通字符 */
		if (g_line_len < AT_LINE_BUF_SIZE - 1)
		{
			g_line_buf[g_line_len++] = ch;
		}
	}

	return AT_FRAME_INCOMPLETE;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/


/*
 * at_parser_reset_buffer: 复位所有积累状态，准备接收下一帧。
 */
static void at_parser_reset_buffer(void)
{
	g_line_len     = 0;
	g_resp_len     = 0;
	g_resp_status  = AT_RESP_TIMEOUT;
	g_frame_prompt = false;

	memset(g_line_buf, 0, AT_LINE_BUF_SIZE);
	memset(g_resp_buf, 0, AT_RESP_BUF_SIZE);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
