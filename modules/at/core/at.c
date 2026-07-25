/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "at.h"
#include "at_port.h"

#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define AT_INTER_FRAME_MS    200                                /* 帧间超时(ms)                */
#define AT_STREAM_BUF_SIZE   2048                               /* 接收流缓冲大小               */
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
const at_uart_driver_t *g_at_driver;
static bool                 g_initialized;
static at_active_t          g_active;
static StreamBufferHandle_t g_at_stream;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int at_init(const at_uart_driver_t *driver, void *port_cfg)
{
    if (!driver || !driver->init || !driver->send)
    {
        return -1;
    }

    if (g_initialized)
    {
        at_deinit();
    }

    g_at_stream = xStreamBufferCreate(AT_STREAM_BUF_SIZE, 1);
    if (g_at_stream == NULL)
    {
        return -3;
    }

    if (driver->init(port_cfg, at_uart_data_cb, NULL) != 0)
    {
        vStreamBufferDelete(g_at_stream);
        g_at_stream = NULL;
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

    if (g_at_stream)
    {
        vStreamBufferDelete(g_at_stream);
        g_at_stream = NULL;
    }

    g_at_driver   = NULL;
    g_initialized = false;
    g_active.active = false;

    return 0;
}

/**
 * @brief 发送 AT 命令并等待响应，内部实现了发送、接收、帧解析和超时处理。
 * 
 * @param cmd  AT 命令字符串（不包含帧头尾）
 * @param resp  
 * @param timeout_ms 
 * @return at_result_t 
 */
at_result_t at_exec(const char *cmd,at_response_t *resp,uint32_t timeout_ms)
{
    //发送命令
    at_transaction_send((uint8_t *)cmd, strlen(cmd));

    //等待响应（简化实现：实际应包含帧解析和超时处理）

}

/**
 * @brief 发送 AT 命令并等待响应，内部实现了发送、接收、帧解析和超时处理。
 * 
 * @param cmd 
 * @param expect 
 * @param resp 
 * @param timeout_ms 
 * @return at_result_t 
 */
at_result_t at_exec_ex(const char *cmd, const char *expect, at_response_t *resp, uint32_t timeout_ms)
{
    //发送命令
    at_transaction_send((uint8_t *)cmd, strlen(cmd));

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
/*							Static Functions    						    */
/****************************************************************************/

/*
 * Port 层接收回调：由 port 的 RX task 调用，将接收到的原始数据写入流缓冲。
 * 此回调在 port 层 task 上下文执行，仅做轻量数据搬运，不涉及解析。
 */
static void at_uart_data_cb(const uint8_t *data, uint32_t len, void *user_ctx)
{
    (void)user_ctx;

    if (g_at_stream)
    {
        xStreamBufferSend(g_at_stream, data, len, 0);
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
