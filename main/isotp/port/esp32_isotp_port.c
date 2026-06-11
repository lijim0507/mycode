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

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct
{
    uint8_t initialized;
} isotp_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int      esp32_isotp_init(void);
static int      esp32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len);
static int      esp32_isotp_deinit(void);
static uint32_t esp32_isotp_get_ms(void);
static void     esp32_isotp_debug(const char *message, ...);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static isotp_esp32_ctx_t g_isotp_ctx;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const isotp_port_driver_t *isotp_port_get_driver(void)
{
    static const isotp_port_driver_t driver = {
        .init   = esp32_isotp_init,
        .send   = esp32_isotp_send,
        .deinit = esp32_isotp_deinit,
        .get_ms = esp32_isotp_get_ms,
        .debug  = esp32_isotp_debug,
    };
    return &driver;
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
        .alerts_enabled = TWAI_ALERT_NONE,
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

    g_isotp_ctx.initialized = 1;
    return 0;
}

static int esp32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (!g_isotp_ctx.initialized)
    {
        return -1;
    }

    twai_message_t tx_msg = {
        .identifier       = id,
        .data_length_code = (len > 8) ? 8 : len,
        .extd             = 0,
        .rtr              = 0,
    };

    if (data && len > 0)
    {
        memcpy(tx_msg.data, data, tx_msg.data_length_code);
    }

    if (twai_transmit(&tx_msg, pdMS_TO_TICKS(100)) != ESP_OK)
    {
        return -2;
    }

    return 0;
}

static int esp32_isotp_deinit(void)
{
    if (!g_isotp_ctx.initialized)
    {
        return 0;
    }

    twai_stop();
    twai_driver_uninstall();
    g_isotp_ctx.initialized = 0;
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