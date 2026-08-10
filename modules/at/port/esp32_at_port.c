/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "at_port.h"

#include <stdbool.h>

#include "driver/uart.h"
#include "driver/gpio.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#ifndef AT_ESP32_UART_PORT
#define AT_ESP32_UART_PORT       UART_NUM_2
#endif

#ifndef AT_ESP32_UART_BAUDRATE
#define AT_ESP32_UART_BAUDRATE   115200
#endif

#ifndef AT_ESP32_UART_TX_PIN
#define AT_ESP32_UART_TX_PIN     17
#endif

#ifndef AT_ESP32_UART_RX_PIN
#define AT_ESP32_UART_RX_PIN     16
#endif

#ifndef AT_ESP32_UART_BUF_SIZE
#define AT_ESP32_UART_BUF_SIZE   1024
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef struct
{
    bool initialized;
} at_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  esp32_at_init(void);
static int  esp32_at_deinit(void);
static int  esp32_at_send(const uint8_t *data, uint32_t len);
static int  esp32_at_receive(uint8_t *data, uint32_t *len, uint32_t timeout_ms);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static at_esp32_ctx_t g_at_ctx;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const uart_port_driver_t *at_port_get_driver(void)
{
    static const uart_port_driver_t driver =
    {
        .init    = esp32_at_init,
        .deinit  = esp32_at_deinit,
        .send    = esp32_at_send,
        .receive = esp32_at_receive,
    };
    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 ESP32 UART（poll 模式）
 *
 * 所有硬件参数由文件顶部宏定义，编译时可通过 -D 覆盖。
 *
 * @return 0: 成功, -1: 驱动安装失败, -2: 参数配置失败, -3: 引脚设置失败
 */
static int esp32_at_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = AT_ESP32_UART_BAUDRATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_driver_install(AT_ESP32_UART_PORT, AT_ESP32_UART_BUF_SIZE,
                            AT_ESP32_UART_BUF_SIZE, 0, NULL, 0) != ESP_OK)
    {
        return -1;
    }

    if (uart_param_config(AT_ESP32_UART_PORT, &uart_cfg) != ESP_OK)
    {
        uart_driver_delete(AT_ESP32_UART_PORT);
        return -2;
    }

    if (uart_set_pin(AT_ESP32_UART_PORT, AT_ESP32_UART_TX_PIN,
                     AT_ESP32_UART_RX_PIN,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
    {
        uart_driver_delete(AT_ESP32_UART_PORT);
        return -3;
    }

    g_at_ctx.initialized = true;
    return 0;
}

/**
 * @brief  反初始化 ESP32 UART
 * @return 0: 成功
 */
static int esp32_at_deinit(void)
{
    if (!g_at_ctx.initialized)
    {
        return 0;
    }

    uart_driver_delete(AT_ESP32_UART_PORT);
    g_at_ctx.initialized = false;
    return 0;
}

/**
 * @brief  UART 发送数据
 * @return 0: 成功, -1: 未初始化, -2: 发送失败
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

/**
 * @brief  UART 接收数据（poll 模式）
 *
 * @param  data        接收缓冲区
 * @param  len         输入：期望长度，输出：实际接收长度
 * @param  timeout_ms  超时（0 = 非阻塞，立即返回 FIFO 中已有数据）
 * @return 实际接收字节数, -1: 未初始化
 */
static int esp32_at_receive(uint8_t *data, uint32_t *len, uint32_t timeout_ms)
{
    if (!g_at_ctx.initialized || !data || !len || *len == 0)
    {
        return -1;
    }

    int ret = uart_read_bytes(AT_ESP32_UART_PORT, data, *len,
                               pdMS_TO_TICKS(timeout_ms));
    if (ret <= 0)
    {
        *len = 0;
        return 0;
    }

    *len = (uint32_t)ret;
    return ret;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
