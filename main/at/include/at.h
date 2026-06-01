
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

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/* AT 响应状态 */
typedef enum
{
    AT_RESP_OK = 0,                                          /* 指令执行成功                 */
    AT_RESP_ERROR,                                           /* 指令执行失败                 */
    AT_RESP_TIMEOUT,                                         /* 超时无响应                   */
    AT_RESP_BUSY,                                            /* 模块忙                       */
} at_resp_status_t;

/* AT 响应结构，存放模块返回的完整响应数据 */
typedef struct
{
    at_resp_status_t status;                                /* 响应状态                     */
    char             data[AT_RESP_BUF_SIZE];                /* 响应原始数据                 */
    uint32_t         data_len;                              /* 有效数据长度                 */
} at_resp_t;

/* 异步命令完成回调 (命令响应结束或超时时调用) */
typedef void (*at_async_callback_t)(at_resp_status_t status,
                                    const char *data, uint32_t data_len,
                                    void *user_data);

/* URC 行回调 (收到主动上报时调用) */
typedef void (*at_urc_handler_t)(const char *line, void *user_data);

/* 接收数据回调：port 层收到 UART 数据后调用此回调上抛给 core 层，运行在 port 的 RX task 上下文 */
typedef void (*at_uart_recv_cb_t)(const uint8_t *data, uint32_t len, void *user_ctx);

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

/* ---- URC 注册 ---- */
int at_urc_register(const char *prefix, at_urc_handler_t handler, void *user_data);
int at_urc_unregister(const char *prefix);

/* ---- 阻塞 API (同步) ---- */
int at_send_command(const char *cmd, at_resp_t *resp, uint32_t timeout_ms);
int at_send_command_data(const char *cmd, const uint8_t *data, uint32_t data_len,
                         at_resp_t *resp, uint32_t timeout_ms);

/* ---- 非阻塞 API (异步) ---- */
int at_send_async(const char *cmd, at_async_callback_t cb, void *user_data,
                  uint32_t timeout_ms);
int at_send_data_async(const char *cmd, const uint8_t *data, uint32_t data_len,
                       at_async_callback_t cb, void *user_data, uint32_t timeout_ms);
int at_recv_poll(void);     /* 在 Task 循环中周期性调用，驱动接收和解析       */

/* ---- 流缓冲接口 (core/at.c) ---- */
int at_stream_recv(uint8_t *buf, uint32_t buf_size, uint32_t timeout_ms);

/* ---- 底层帧接口 (core/at_frame.c) ---- */
int at_frame_send(const uint8_t *content, uint32_t content_len);
int at_frame_recv(at_resp_t *resp, uint32_t timeout_ms);
int at_frame_send_cmd(const char *cmd, at_resp_t *resp, uint32_t timeout_ms);

/* ---- 解析器接口 (core/at_parser.c) ---- */
/* 阶段一：帧完成性检测 */
int              at_parser_feed(const char *data, uint32_t len);
int              at_parser_check_timeout(uint32_t timeout_ms);
void             at_parser_get_frame(char *buf, uint32_t *out_len);
/* 阶段二：帧内容分析 */
at_resp_status_t at_parser_analyze(const char *frame, uint32_t frame_len,
                                    char *resp_data, uint32_t *resp_len);
int              at_parser_urc_register(const char *prefix, at_urc_handler_t handler,
                                         void *user_data);
int              at_parser_urc_unregister(const char *prefix);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
