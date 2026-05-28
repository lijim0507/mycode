/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds.h"
#include "uds_port.h"
#include "isotp.h"

#include <string.h>
#include <stdarg.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static uint8_t g_send_buf[4096];
static uint8_t g_recv_buf[4096];
/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const uds_can_driver_t *g_driver;
static bool                    g_initialized;
static isotp_handle_t          g_isotp;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int uds_init(const uds_can_driver_t *driver, void *port_cfg)
{
    if (!driver || !driver->send || !driver->get_ms) {
        return -1;
    }

    if (g_initialized) {
        uds_deinit();
    }

    if (driver->init && driver->init(port_cfg) != 0) {
        return -2;
    }

    g_driver = driver;

    isotp_init_link(&g_isotp, 0x7E0, g_send_buf, sizeof(g_send_buf),
                    g_recv_buf, sizeof(g_recv_buf));

    g_initialized = true;
    return 0;
}

int uds_deinit(void)
{
    if (!g_initialized) {
        return 0;
    }

    if (g_driver && g_driver->deinit) {
        g_driver->deinit();
    }

    g_driver      = NULL;
    g_initialized = false;
    return 0;
}

void uds_poll(void)
{
    if (!g_initialized) {
        return;
    }
    isotp_poll(&g_isotp);
}

void uds_on_can_frame(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (!g_initialized) {
        return;
    }
    isotp_on_can_message(&g_isotp, (uint8_t *)data, len);
}

/****************************************************************************/
/*							ISOTP User Shims                                */
/****************************************************************************/

void isotp_user_debug(const char *message, ...)
{
    if (g_driver && g_driver->debug) {
        va_list args;
        va_start(args, message);
        /* delegate to driver's debug function */
        va_end(args);
    }
}

int isotp_user_send_can(const uint32_t arbitration_id,
                         const uint8_t *data, const uint8_t size)
{
    if (g_driver && g_driver->send) {
        return g_driver->send(arbitration_id, data, size) == 0
               ? 0 : -1;
    }
    return -1;
}

uint32_t isotp_user_get_ms(void)
{
    if (g_driver && g_driver->get_ms) {
        return g_driver->get_ms();
    }
    return 0;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
