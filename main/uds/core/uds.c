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

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static uint8_t g_send_buf[4096];
static uint8_t g_recv_buf[4096];

static const uds_can_driver_t *g_driver;
static uint8_t                 g_initialized;
static isotp_handle_t          g_isotp;

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

    if (isotp_init() != ISOTP_RET_OK)
    {
        if (g_driver->deinit)
        {
            g_driver->deinit();
        }
        g_driver = NULL;
        return -3;
    }

    isotp_init_handle(&g_isotp, 0x7E8, 0x7E0,
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
    isotp_feed(&g_isotp, id, (uint8_t *)data, len);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/