
#ifndef __UART_FRAME_H_
#define __UART_FRAME_H_
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
#define UART_FRAME_BUF_SIZE    512                              /* 默认帧缓冲大小               */

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef enum
{
    UART_FRAME_STATE_IDLE     = 0,
    UART_FRAME_STATE_COLLECT,
    UART_FRAME_STATE_COMPLETE,
} uart_frame_state_t;

typedef struct
{
    uart_frame_state_t state;
    uint8_t            buf[UART_FRAME_BUF_SIZE];
    uint16_t           buf_len;
    uint32_t           timeout_ms;
    uint32_t           last_tick;
} uart_frame_parser_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

void     uart_frame_init(uart_frame_parser_t *p, uint32_t timeout_ms);
int      uart_frame_feed(uart_frame_parser_t *p, const uint8_t *data,
                         uint8_t len, uint32_t tick);
int      uart_frame_check_timeout(uart_frame_parser_t *p, uint32_t tick);
int      uart_frame_is_complete(uart_frame_parser_t *p);
void     uart_frame_reset(uart_frame_parser_t *p);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
