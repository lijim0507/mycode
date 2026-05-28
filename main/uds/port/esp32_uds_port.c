/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds_port.h"

#include <string.h>

#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define TAG "UDS_PORT"

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct {
    uint8_t tx_gpio;
    uint8_t rx_gpio;
} uds_esp32_cfg_t;

typedef struct {
    bool initialized;
} uds_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int      esp32_uds_init(void *config);
static int      esp32_uds_send(uint32_t id, const uint8_t *data, uint8_t len);
static int      esp32_uds_deinit(void);
static uint32_t esp32_uds_get_ms(void);
static void     esp32_uds_debug(const char *message, ...);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static uds_esp32_ctx_t g_uds_ctx;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const uds_can_driver_t *uds_port_get_driver(void)
{
    static const uds_can_driver_t driver = {
        .init   = esp32_uds_init,
        .send   = esp32_uds_send,
        .deinit = esp32_uds_deinit,
        .get_ms = esp32_uds_get_ms,
        .debug  = esp32_uds_debug,
    };
    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int esp32_uds_init(void *config)
{
    uint8_t tx_gpio = GPIO_NUM_21;
    uint8_t rx_gpio = GPIO_NUM_22;

    if (config != NULL) {
        uds_esp32_cfg_t *cfg = (uds_esp32_cfg_t *)config;
        tx_gpio = cfg->tx_gpio;
        rx_gpio = cfg->rx_gpio;
    }

    twai_general_config_t g_cfg = {
        .mode         = TWAI_MODE_NORMAL,
        .tx_io        = (gpio_num_t)tx_gpio,
        .rx_io        = (gpio_num_t)rx_gpio,
        .clkout_io    = GPIO_NUM_NC,
        .bus_off_io   = GPIO_NUM_NC,
        .tx_queue_len = 16,
        .rx_queue_len = 16,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider  = 0,
    };

    twai_timing_config_t t_cfg = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK) {
        return -1;
    }

    if (twai_start() != ESP_OK) {
        twai_driver_uninstall();
        return -2;
    }

    g_uds_ctx.initialized = true;
    return 0;
}

static int esp32_uds_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (!g_uds_ctx.initialized) {
        return -1;
    }

    twai_message_t tx_msg = {
        .identifier       = id,
        .data_length_code = (len > 8) ? 8 : len,
        .extd             = false,
        .rtr              = false,
    };

    if (data && len > 0) {
        memcpy(tx_msg.data, data, tx_msg.data_length_code);
    }

    if (twai_transmit(&tx_msg, pdMS_TO_TICKS(100)) != ESP_OK) {
        return -2;
    }

    return 0;
}

static int esp32_uds_deinit(void)
{
    if (!g_uds_ctx.initialized) {
        return 0;
    }

    twai_stop();
    twai_driver_uninstall();
    g_uds_ctx.initialized = false;
    return 0;
}

static uint32_t esp32_uds_get_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void esp32_uds_debug(const char *message, ...)
{
    (void)message;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
