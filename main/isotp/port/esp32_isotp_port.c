/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "isotp_port.h"

#include <string.h>

#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define TAG "ISOTP_PORT"

#ifndef ISOTP_ESP32_TWAI_TX_GPIO
#define ISOTP_ESP32_TWAI_TX_GPIO    GPIO_NUM_21
#endif

#ifndef ISOTP_ESP32_TWAI_RX_GPIO
#define ISOTP_ESP32_TWAI_RX_GPIO    GPIO_NUM_22
#endif

#ifndef ISOTP_ESP32_RX_BUF_SIZE
#define ISOTP_ESP32_RX_BUF_SIZE     16
#endif

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct
{
    uint32_t id;
    uint8_t  data[8];
    uint8_t  dlc;
} isotp_esp32_rx_frame_t;

typedef struct
{
    volatile uint32_t head;
    volatile uint32_t tail;
    isotp_esp32_rx_frame_t frames[ISOTP_ESP32_RX_BUF_SIZE];
} isotp_esp32_rx_buf_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int      esp32_isotp_init(void);
static int      esp32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len);
static int      esp32_isotp_receive(uint32_t *id, uint8_t *data, uint8_t *len);
static int      esp32_isotp_deinit(void);
static uint32_t esp32_isotp_get_ms(void);
static void     esp32_isotp_debug(const char *message, ...);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static isotp_esp32_rx_buf_t g_rx_buf;
static uint8_t g_initialized;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/
/*
 * Usage:
 *
 * 1) Task context — TWAI alert callback or dedicated RX task:
 *
 *    void isotp_rx_task(void *arg)
 *    {
 *        twai_rx_msg_t rx_msg;
 *        for (;;)
 *        {
 *            if (twai_receive(&rx_msg, pdMS_TO_TICKS(100)) == ESP_OK)
 *            {
 *                esp32_isotp_twai_rx_callback(rx_msg.identifier, rx_msg.data, rx_msg.data_length_code);
 *            }
 *        }
 *    }
 *
 * 2) Main loop — isotp_poll() will drain the ring buffer automatically:
 *
 *    isotp_poll(&handle);
 */

const isotp_port_driver_t *isotp_port_get_driver(void)
{
    static const isotp_port_driver_t driver = {
        .init    = esp32_isotp_init,
        .send    = esp32_isotp_send,
        .receive = esp32_isotp_receive,
        .deinit  = esp32_isotp_deinit,
        .get_ms  = esp32_isotp_get_ms,
        .debug   = esp32_isotp_debug,
    };
    return &driver;
}

void esp32_isotp_twai_rx_callback(uint32_t id, const uint8_t *data, uint8_t len)
{
    uint32_t next;

    if (len > 8)
    {
        len = 8;
    }

    next = (g_rx_buf.head + 1) % ISOTP_ESP32_RX_BUF_SIZE;

    if (next == g_rx_buf.tail)
    {
        return;
    }

    g_rx_buf.frames[g_rx_buf.head].id = id;
    (void)memcpy(g_rx_buf.frames[g_rx_buf.head].data, data, len);
    g_rx_buf.frames[g_rx_buf.head].dlc = len;

    g_rx_buf.head = next;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int esp32_isotp_init(void)
{
    twai_general_config_t g_cfg = {
        .mode           = TWAI_MODE_NORMAL,
        .tx_io          = ISOTP_ESP32_TWAI_TX_GPIO,
        .rx_io          = ISOTP_ESP32_TWAI_RX_GPIO,
        .clkout_io      = GPIO_NUM_NC,
        .bus_off_io     = GPIO_NUM_NC,
        .tx_queue_len   = 16,
        .rx_queue_len   = 16,
        .alerts_enabled  = TWAI_ALERT_NONE,
        .clkout_divider = 0,
    };

    twai_timing_config_t t_cfg = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK)
    {
        return -1;
    }

    if (twai_start() != ESP_OK)
    {
        twai_driver_uninstall();
        return -2;
    }

    g_rx_buf.head = 0;
    g_rx_buf.tail = 0;
    g_initialized = 1;

    return 0;
}

static int esp32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    twai_message_t tx_msg;

    if (!g_initialized)
    {
        return -1;
    }

    tx_msg.identifier       = id;
    tx_msg.data_length_code = (len > 8) ? 8 : len;
    tx_msg.extd             = 0;
    tx_msg.rtr              = 0;

    (void)memset(tx_msg.data, 0, sizeof(tx_msg.data));
    if (data && len > 0)
    {
        (void)memcpy(tx_msg.data, data, tx_msg.data_length_code);
    }

    if (twai_transmit(&tx_msg, pdMS_TO_TICKS(100)) != ESP_OK)
    {
        return -2;
    }

    return 0;
}

static int esp32_isotp_receive(uint32_t *id, uint8_t *data, uint8_t *len)
{
    if (g_rx_buf.head == g_rx_buf.tail)
    {
        return 0;
    }

    if (id != NULL)
    {
        *id = g_rx_buf.frames[g_rx_buf.tail].id;
    }

    if (data != NULL && len != NULL)
    {
        *len = g_rx_buf.frames[g_rx_buf.tail].dlc;
        (void)memcpy(data, g_rx_buf.frames[g_rx_buf.tail].data, g_rx_buf.frames[g_rx_buf.tail].dlc);
    }

    g_rx_buf.tail = (g_rx_buf.tail + 1) % ISOTP_ESP32_RX_BUF_SIZE;

    return 1;
}

static int esp32_isotp_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    twai_stop();
    twai_driver_uninstall();

    g_initialized = 0;
    g_rx_buf.head = 0;
    g_rx_buf.tail = 0;

    return 0;
}

static uint32_t esp32_isotp_get_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void esp32_isotp_debug(const char *message, ...)
{
    (void)message;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/