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

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int      esp32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len);
static uint32_t esp32_isotp_get_ms(void);
static void     esp32_isotp_debug(const char *message, ...);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const isotp_port_driver_t *isotp_port_get_driver(void)
{
    static const isotp_port_driver_t driver = {
        .send   = esp32_isotp_send,
        .get_ms = esp32_isotp_get_ms,
        .debug  = esp32_isotp_debug,
    };
    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int esp32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    twai_message_t tx_msg;

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