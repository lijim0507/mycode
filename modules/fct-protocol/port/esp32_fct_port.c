/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "fct_port.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "driver/uart.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#ifndef FCT_ESP32_UART_PORT
#define FCT_ESP32_UART_PORT         UART_NUM_0
#endif

#ifndef FCT_ESP32_UART_BAUDRATE
#define FCT_ESP32_UART_BAUDRATE     115200
#endif

#ifndef FCT_ESP32_UART_TX_PIN
#define FCT_ESP32_UART_TX_PIN       17
#endif

#ifndef FCT_ESP32_UART_RX_PIN
#define FCT_ESP32_UART_RX_PIN       16
#endif

#ifndef FCT_ESP32_UART_TX_BUF_SIZE
#define FCT_ESP32_UART_TX_BUF_SIZE  512
#endif

#ifndef FCT_ESP32_UART_RX_BUF_SIZE
#define FCT_ESP32_UART_RX_BUF_SIZE  256
#endif

#ifndef FCT_ESP32_UART_TX_TIMEOUT_MS
#define FCT_ESP32_UART_TX_TIMEOUT_MS 100
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef struct
{
    bool initialized;
} fct_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int esp32_fct_init(void);
static int esp32_fct_deinit(void);
static int esp32_fct_send(const uint8_t *data, uint32_t len);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static fct_esp32_ctx_t g_fct_ctx;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  获取 ESP32 串口驱动实例
 *
 * @return 指向静态 const fct_port_driver_t 的指针
 */
const fct_port_driver_t *fct_port_get_driver(void)
{
    static const fct_port_driver_t driver =
    {
        .init   = esp32_fct_init,
        .deinit = esp32_fct_deinit,
        .send   = esp32_fct_send,
    };

    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 ESP32 UART
 *
 * 只用于发送；端口、波特率、引脚由文件顶部宏定义，编译时可通过 -D 覆盖。
 *
 * @return 0: 成功, -1: 参数配置失败, -2: 引脚设置失败, -3: 驱动安装失败
 */
static int esp32_fct_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = FCT_ESP32_UART_BAUDRATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (g_fct_ctx.initialized)
    {
        esp32_fct_deinit();
    }

    if (uart_param_config(FCT_ESP32_UART_PORT, &uart_cfg) != ESP_OK)
    {
        return -1;
    }

    if (uart_set_pin(FCT_ESP32_UART_PORT, FCT_ESP32_UART_TX_PIN,
                     FCT_ESP32_UART_RX_PIN,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
    {
        return -2;
    }

    if (uart_driver_install(FCT_ESP32_UART_PORT, FCT_ESP32_UART_RX_BUF_SIZE,
                            FCT_ESP32_UART_TX_BUF_SIZE, 0, NULL, 0) != ESP_OK)
    {
        return -3;
    }

    g_fct_ctx.initialized = true;
    return 0;
}

/**
 * @brief  反初始化 ESP32 UART
 *
 * @return 0: 成功
 */
static int esp32_fct_deinit(void)
{
    if (!g_fct_ctx.initialized)
    {
        return 0;
    }

    uart_driver_delete(FCT_ESP32_UART_PORT);
    g_fct_ctx.initialized = false;
    return 0;
}

/**
 * @brief  发送一帧报文并等待发送完成
 *
 * @param  data 帧数据，含 CRLF
 * @param  len  帧长度
 * @return 0: 成功, -1: 未初始化或参数错误, -2: 写入失败, -3: 等待发送完成超时
 * @note   等待发送完成保证一帧完整占用串口后再返回，避免多帧交错。
 */
static int esp32_fct_send(const uint8_t *data, uint32_t len)
{
    int ret;

    if (!g_fct_ctx.initialized || data == NULL || len == 0)
    {
        return -1;
    }

    ret = uart_write_bytes(FCT_ESP32_UART_PORT, (const char *)data, len);
    if (ret < 0 || (uint32_t)ret != len)
    {
        return -2;
    }

    if (uart_wait_tx_done(FCT_ESP32_UART_PORT,
                          pdMS_TO_TICKS(FCT_ESP32_UART_TX_TIMEOUT_MS)) != ESP_OK)
    {
        return -3;
    }

    return 0;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
