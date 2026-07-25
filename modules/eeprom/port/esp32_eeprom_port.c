/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "eeprom_port.h"

#include <string.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define EEPROM_SPI_HOST  SPI3_HOST
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct {
    spi_device_handle_t spi_handle;
    uint8_t             cs_gpio;
} esp32_eeprom_spi_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static void    esp32_spi_cs_set(uint8_t level);
static uint8_t esp32_spi_transfer(uint8_t data);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static esp32_eeprom_spi_ctx_t g_spi_ctx;
static bool                   g_spi_initialized;

static spi_eeprom_transport_t g_spi_transport;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const i2c_transport_t *eeprom_port_get_i2c(void)
{
    return swi2c_get_transport();
}

const spi_eeprom_transport_t *eeprom_port_get_spi(void)
{
    if (!g_spi_initialized) {
        return NULL;
    }
    return &g_spi_transport;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static void esp32_spi_cs_set(uint8_t level)
{
    gpio_set_level((gpio_num_t)g_spi_ctx.cs_gpio, level);
}

static uint8_t esp32_spi_transfer(uint8_t data)
{
    spi_transaction_t trans = {
        .length    = 8,
        .tx_buffer = &data,
        .rx_buffer = &data,
    };

    spi_device_polling_transmit(g_spi_ctx.spi_handle, &trans);
    return data;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
