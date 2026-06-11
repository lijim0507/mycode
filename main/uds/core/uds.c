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

static int      uds_isotp_send(uint32_t id, const uint8_t *data, uint8_t len);
static uint32_t uds_isotp_get_ms(void);
static void     uds_isotp_debug(const char *message, ...);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static uint8_t g_send_buf[4096];
static uint8_t g_recv_buf[4096];

static const uds_can_driver_t *g_driver;
static uint8_t                 g_initialized;
static isotp_handle_t          g_isotp;
static isotp_port_driver_t     g_isotp_port;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int uds_init(const uds_can_driver_t *driver)
{
    if (!driver || !driver->send || !driver->get_ms)
    {
        return -1;
    }

    if (g_initialized)
    {
        uds_deinit();
    }

    g_driver = driver;

    if (driver->init && driver->init() != 0)
    {
        g_driver = NULL;
        return -2;
    }

    g_isotp_port.send   = uds_isotp_send;
    g_isotp_port.get_ms = uds_isotp_get_ms;
    g_isotp_port.debug  = uds_isotp_debug;

    if (isotp_init(&g_isotp_port) != ISOTP_RET_OK)
    {
        if (g_driver->deinit)
        {
            g_driver->deinit();
        }
        g_driver = NULL;
        return -3;
    }

    isotp_init_link(&g_isotp, 0x7E0, 0x7E8,
                    g_send_buf, sizeof(g_send_buf),
                    g_recv_buf, sizeof(g_recv_buf));

    g_initialized = 1;
    return 0;
}

int uds_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    isotp_deinit();

    if (g_driver && g_driver->deinit)
    {
        g_driver->deinit();
    }

    g_driver      = NULL;
    g_initialized = 0;
    return 0;
}

void uds_poll(void)
{
    if (!g_initialized)
    {
        return;
    }
    isotp_poll(&g_isotp);
}

void uds_on_can_frame(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (!g_initialized)
    {
        return;
    }
    isotp_on_can_message(&g_isotp, (uint8_t *)data, len);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int uds_isotp_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (g_driver && g_driver->send)
    {
        return g_driver->send(id, data, len) == 0 ? 0 : -1;
    }
    return -1;
}

static uint32_t uds_isotp_get_ms(void)
{
    if (g_driver && g_driver->get_ms)
    {
        return g_driver->get_ms();
    }
    return 0;
}

static void uds_isotp_debug(const char *message, ...)
{
    if (g_driver && g_driver->debug)
    {
        va_list args;
        va_start(args, message);
        g_driver->debug(message, args);
        va_end(args);
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/