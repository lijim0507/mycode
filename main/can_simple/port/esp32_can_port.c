/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "can_simple_port.h"

#include <string.h>

#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define CAN_SIMPLE_TX_TIMEOUT_MS  100
#define CAN_SIMPLE_RX_TIMEOUT_MS  100
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct {
    twai_timing_config_t  timing;
    twai_filter_config_t  filter;
    uint8_t               tx_gpio;
    uint8_t               rx_gpio;
} can_simple_esp32_cfg_t;

typedef struct {
    bool initialized;
} can_simple_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  esp32_can_init(void *config);
static int  esp32_can_send(uint16_t id, const uint8_t *data, uint8_t len);
static int  esp32_can_recv(uint16_t *id, uint8_t *data, uint8_t *len, uint32_t timeout_ms);
static int  esp32_can_deinit(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static can_simple_esp32_ctx_t g_can_ctx;
/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int esp32_can_init(void *config)
{
    can_simple_esp32_cfg_t *cfg = (can_simple_esp32_cfg_t *)config;
    twai_general_config_t g_cfg;
    twai_timing_config_t  t_cfg;
    twai_filter_config_t  f_cfg;

    if (cfg != NULL) {
        g_cfg = (twai_general_config_t){
            .mode         = TWAI_MODE_NORMAL,
            .tx_io        = (gpio_num_t)cfg->tx_gpio,
            .rx_io        = (gpio_num_t)cfg->rx_gpio,
            .clkout_io    = GPIO_NUM_NC,
            .bus_off_io   = GPIO_NUM_NC,
            .tx_queue_len = 16,
            .rx_queue_len = 16,
            .alerts_enabled = TWAI_ALERT_NONE,
            .clkout_divider  = 0,
        };
        t_cfg = cfg->timing;
        f_cfg = cfg->filter;
    } else {
        g_cfg = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_21, GPIO_NUM_22, TWAI_MODE_NORMAL);
        t_cfg = TWAI_TIMING_CONFIG_500KBITS();
        f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    }

    if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK) {
        return -1;
    }

    if (twai_start() != ESP_OK) {
        twai_driver_uninstall();
        return -2;
    }

    g_can_ctx.initialized = true;
    return 0;
}

static int esp32_can_send(uint16_t id, const uint8_t *data, uint8_t len)
{
    if (!g_can_ctx.initialized) {
        return -1;
    }

    twai_message_t tx_msg = {
        .identifier          = id,
        .data_length_code    = (len > 8) ? 8 : len,
        .extd                = false,
        .rtr                 = false,
        .self                = false,
        .ss                  = true,
    };

    if (data != NULL && len > 0) {
        memcpy(tx_msg.data, data, tx_msg.data_length_code);
    }

    if (twai_transmit(&tx_msg, pdMS_TO_TICKS(CAN_SIMPLE_TX_TIMEOUT_MS)) != ESP_OK) {
        return -2;
    }

    return 0;
}

static int esp32_can_recv(uint16_t *id, uint8_t *data, uint8_t *len, uint32_t timeout_ms)
{
    twai_message_t rx_msg;

    if (!g_can_ctx.initialized) {
        return -1;
    }

    if (twai_receive(&rx_msg, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) {
        return -2;
    }

    if (id != NULL) {
        *id = (uint16_t)rx_msg.identifier;
    }

    if (data != NULL && len != NULL) {
        *len = rx_msg.data_length_code;
        memcpy(data, rx_msg.data, rx_msg.data_length_code);
    }

    return 0;
}

static int esp32_can_deinit(void)
{
    if (!g_can_ctx.initialized) {
        return 0;
    }

    twai_stop();
    twai_driver_uninstall();

    g_can_ctx.initialized = false;
    return 0;
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const can_simple_driver_t *can_simple_port_get_driver(void)
{
    static const can_simple_driver_t driver = {
        .init   = esp32_can_init,
        .send   = esp32_can_send,
        .recv   = esp32_can_recv,
        .deinit = esp32_can_deinit,
    };
    return &driver;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
