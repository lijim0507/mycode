/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uart_frame.h"

#include <string.h>
#include <stdbool.h>
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static int uart_frame_check_format(uart_frame_parser_t *p);
static int uart_frame_overflow_check(uart_frame_parser_t *p, uint8_t len);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/*
 * 初始化解析器。
 */
void uart_frame_init(uart_frame_parser_t *p, uint32_t timeout_ms)
{
    if (!p)
    {
        return;
    }

    memset(p, 0, sizeof(*p));
    p->timeout_ms = timeout_ms;
}

/*
 * 喂入数据块，累积后调用格式检测。
 * 返回值: 1=帧完整, 0=继续收集, -1=溢出或空指针
 */
int uart_frame_feed(uart_frame_parser_t *p, const uint8_t *data,
                    uint8_t len, uint32_t tick)
{
    if (!p || !data || len == 0)
    {
        return -1;
    }

    p->last_tick = tick;

    if (p->state == UART_FRAME_STATE_COMPLETE)
    {
        uart_frame_reset(p);
    }

    if (p->state == UART_FRAME_STATE_IDLE)
    {
        p->state = UART_FRAME_STATE_COLLECT;
    }

    if (uart_frame_overflow_check(p, len))
    {
        return -1;
    }

    memcpy(&p->buf[p->buf_len], data, len);
    p->buf_len += len;

    if (uart_frame_check_format(p))
    {
        p->state = UART_FRAME_STATE_COMPLETE;
        return 1;
    }

    return 0;
}

/*
 * 检查帧间超时。
 * 返回值: 1=超时触发(帧完整), 0=未超时
 */
int uart_frame_check_timeout(uart_frame_parser_t *p, uint32_t tick)
{
    if (!p || p->state != UART_FRAME_STATE_COLLECT)
    {
        return 0;
    }

    if (p->buf_len == 0)
    {
        return 0;
    }

    if (tick - p->last_tick >= p->timeout_ms)
    {
        p->state = UART_FRAME_STATE_COMPLETE;
        return 1;
    }

    return 0;
}

/*
 * 检查当前帧是否完整（状态查询）。
 */
int uart_frame_is_complete(uart_frame_parser_t *p)
{
    if (!p)
    {
        return 0;
    }

    return (p->state == UART_FRAME_STATE_COMPLETE) ? 1 : 0;
}

/*
 * 复位解析器。
 */
void uart_frame_reset(uart_frame_parser_t *p)
{
    if (!p)
    {
        return;
    }

    p->state   = UART_FRAME_STATE_IDLE;
    p->buf_len = 0;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/*
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │ [移植适配点] 根据目标协议替换此函数 !                                    │
 * │                                                                      │
 * │ 返回值: 1 = 当前 buf 中的数据构成一个完整帧                              │
 * │ 示例:                                                                 │
 * │   AT 协议:  strstr(p->buf, "OK\r\n") 或 strstr(p->buf, "ERROR\r\n")    │
 * │   Modbus:   固定返回 0，完全依赖 uart_frame_check_timeout              │
 * │   定长协议:  return p->buf_len >= EXPECTED_LEN;                       │
 * │   "\r\n"行: return memchr(p->buf, '\n', p->buf_len) ? 1 : 0;         │
 * └──────────────────────────────────────────────────────────────────────┘
 */
static int uart_frame_check_format(uart_frame_parser_t *p)
{
    /* ---- [替换此处的协议检测逻辑] ---- */
    (void)p;
    return 0;
}

/*
 * 防溢出检查。
 */
static int uart_frame_overflow_check(uart_frame_parser_t *p, uint8_t len)
{
    if (p->buf_len + len > UART_FRAME_BUF_SIZE)
    {
        /* 溢出时重置缓冲区，丢弃当前帧 */
        uart_frame_reset(p);
        return -1;
    }

    return 0;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
