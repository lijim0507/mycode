/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "at_port.h"

#include <string.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define AT_ESP32_UART_PORT       UART_NUM_2     /* 使用 ESP32 UART2           */
#define AT_ESP32_UART_BAUDRATE   115200         /* 默认波特率                  */
#define AT_ESP32_UART_TX_PIN     17             /* 默认 TX 引脚                */
#define AT_ESP32_UART_RX_PIN     16             /* 默认 RX 引脚                */
#define AT_ESP32_UART_BUF_SIZE   1024           /* UART 内部环形缓冲区大小      */
/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/* ESP32 AT 端口用户可配置参数 */
typedef struct
{
    uint8_t  tx_pin;       /* TX GPIO 引脚                                   */
    uint8_t  rx_pin;       /* RX GPIO 引脚                                   */
    uint32_t baudrate;     /* 通信波特率                                      */
} at_esp32_cfg_t;

/* ESP32 AT 端口内部上下文 */
typedef struct
{
    bool             initialized;  /* 初始化完成标志                           */
    SemaphoreHandle_t recv_sem;    /* 接收信号量（预留 ISR 通知）              */
} at_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  esp32_at_init(void *config);
static int  esp32_at_send(const uint8_t *data, uint32_t len);
static int  esp32_at_recv(uint8_t *buf, uint32_t buf_size, uint32_t timeout_ms);
static int  esp32_at_deinit(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static at_esp32_ctx_t g_at_ctx;     /* 端口全局上下文                          */

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/*
 * 初始化 ESP32 UART。
 * config 可传入 at_esp32_cfg_t 覆盖默认引脚和波特率，传 NULL 使用默认值。
 * 返回值: 0=成功, -1=驱动安装失败, -2=参数配置失败, -3=引脚设置失败, -4=信号量创建失败
 */
static int esp32_at_init(void *config)
{
    uint8_t  tx_pin   = AT_ESP32_UART_TX_PIN;
    uint8_t  rx_pin   = AT_ESP32_UART_RX_PIN;
    uint32_t baudrate = AT_ESP32_UART_BAUDRATE;

    /* 如果传入了用户配置则覆盖默认值 */
    if (config != NULL)
    {
        at_esp32_cfg_t *cfg = (at_esp32_cfg_t *)config;
        tx_pin   = cfg->tx_pin;
        rx_pin   = cfg->rx_pin;
        baudrate = cfg->baudrate;
    }

    /* UART 参数配置: 8N1 无流控 */
    uart_config_t uart_cfg = {
        .baud_rate  = (int)baudrate,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* 安装 UART 驱动（TX/RX 等大缓冲区） */
    if (uart_driver_install(AT_ESP32_UART_PORT, AT_ESP32_UART_BUF_SIZE,
                            AT_ESP32_UART_BUF_SIZE, 0, NULL, 0) != ESP_OK)
    {
        return -1;
    }

    /* 应用 UART 参数配置 */
    if (uart_param_config(AT_ESP32_UART_PORT, &uart_cfg) != ESP_OK)
    {
        uart_driver_delete(AT_ESP32_UART_PORT);
        return -2;
    }

    /* 绑定 TX/RX 引脚 */
    if (uart_set_pin(AT_ESP32_UART_PORT, tx_pin, rx_pin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
    {
        uart_driver_delete(AT_ESP32_UART_PORT);
        return -3;
    }

    /* 创建接收信号量（供日后 ISR 回调使用） */
    g_at_ctx.recv_sem = xSemaphoreCreateBinary();
    if (g_at_ctx.recv_sem == NULL)
    {
        uart_driver_delete(AT_ESP32_UART_PORT);
        return -4;
    }

    g_at_ctx.initialized = true;
    return 0;
}

/*
 * 通过 UART 发送数据块。
 * 返回值: 0=成功, -1=未初始化或参数无效, -2=发送失败
 */
static int esp32_at_send(const uint8_t *data, uint32_t len)
{
    if (!g_at_ctx.initialized || !data || len == 0)
    {
        return -1;
    }

    int ret = uart_write_bytes(AT_ESP32_UART_PORT, (const char *)data, len);
    return (ret < 0) ? -2 : 0;
}

/*
 * 从 UART 接收数据，带超时控制。
 * 返回值: >=0=接收到的字节数, -1=未初始化或参数无效, -2=接收错误
 */
static int esp32_at_recv(uint8_t *buf, uint32_t buf_size, uint32_t timeout_ms)
{
    if (!g_at_ctx.initialized || !buf || buf_size == 0)
    {
        return -1;
    }

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);

    int len = uart_read_bytes(AT_ESP32_UART_PORT, buf, buf_size, ticks);
    return (len < 0) ? -2 : (int)len;
}

/*
 * 反初始化 ESP32 UART。
 * 安全释放信号量和 UART 驱动资源，可重复调用。
 * 返回值: 0=成功
 */
static int esp32_at_deinit(void)
{
    if (!g_at_ctx.initialized)
    {
        return 0;
    }

    if (g_at_ctx.recv_sem != NULL)
    {
        vSemaphoreDelete(g_at_ctx.recv_sem);
        g_at_ctx.recv_sem = NULL;
    }

    uart_driver_delete(AT_ESP32_UART_PORT);
    g_at_ctx.initialized = false;
    return 0;
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/*
 * 获取 ESP32 AT 端口驱动实例。
 * 返回静态单例，供 at_init() 注入到核心层。
 */
const at_uart_driver_t *at_port_get_driver(void)
{
    static const at_uart_driver_t driver = {
        .init   = esp32_at_init,
        .send   = esp32_at_send,
        .recv   = esp32_at_recv,
        .deinit = esp32_at_deinit,
    };
    return &driver;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
