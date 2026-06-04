
#ifndef __AT_H_
#define __AT_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define AT_RESP_BUF_SIZE    512                              /* 响应缓冲区大小               */
#define AT_CMD_MAX_LEN      256                              /* AT 指令最大长度              */
#define AT_URC_MAX_ENTRIES  16                               /* URC 最大注册数              */
#define AT_RESP_MAX_LINES 16

#define LINE_END_MARKER '\0'
/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/* 帧完成类型 */
typedef enum
{
    AT_FRAME_INCOMPLETE = 0,                                 /* 帧尚未完整，继续积累          */
    AT_FRAME_COMPLETE_RESP = 1,                              /* 响应帧完成 (有 OK/ERROR)     */
    AT_FRAME_COMPLETE_URC = 2,                               /* URC 帧完成 (无结束语, 超时)   */
} at_frame_type_t;


typedef enum
{
    AT_RESULT_OK,
    AT_RESULT_ERROR,
    AT_RESULT_TIMEOUT,
} at_result_t;


typedef struct
{
    char *lines[AT_RESP_MAX_LINES]; //回复的响应可能有多行，每行结尾用'\0'分隔
    uint16_t line_num;
} at_response_t;

/* 异步命令完成回调 (命令响应结束或超时时调用) */
typedef void (*at_async_callback_t)(at_result_t result,
                                    const char *data, uint32_t data_len,
                                    void *user_data);

/* URC 行回调 (收到主动上报时调用) */
typedef void (*at_urc_handler_t)(const char *line, void *user_data);


/* UART 抽象驱动，用于解耦 AT 核心与具体硬件 */
typedef struct
{
    int (*init)(void *config, at_uart_recv_cb_t recv_cb, void *user_ctx);  /* 初始化 UART + 注册接收回调  */
    int (*send)(const uint8_t *data, uint32_t len);                        /* 发送数据                     */
    int (*deinit)(void);                                                   /* 反初始化 UART                */
} at_uart_driver_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/* 由 at.c 初始化的 UART 驱动实例，at_frame.c / at_parser.c 通过此指针操作硬件 */
extern const at_uart_driver_t *g_at_driver;

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

/* ---- 生命周期 ---- */
int at_init(const at_uart_driver_t *driver, void *port_cfg);
int at_deinit(void);


at_result_t at_exec(const char *cmd,at_response_t *resp,uint32_t timeout_ms);
at_result_t at_exec_ex(const char *cmd, const char *expect, at_response_t *resp, uint32_t timeout_ms);

/* ---- URC 注册 ---- */
int at_register_urc(const char *prefix, at_urc_handler_t handler, void *user_data);
int at_unregister_urc(const char *prefix);


#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
