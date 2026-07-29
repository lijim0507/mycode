/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "can_bus.h"
#include "can_bus_port.h"
#include "isotp.h"

#include <stddef.h>
#include <stdint.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/
#ifndef UDS_REQUEST_CAN_ID
#define UDS_REQUEST_CAN_ID      0x7E0
#endif

#ifndef UDS_RESPONSE_CAN_ID
#define UDS_RESPONSE_CAN_ID     0x7E8
#endif
/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                         Prototypes Of Local Functions                    */
/****************************************************************************/

static void can_bus_dispatch(uint32_t id, const uint8_t *data, uint8_t len);
static void can_bus_task(void *arg);

/****************************************************************************/
/*                         Global Variables                                 */
/****************************************************************************/

static const can_port_driver_t *g_can_driver;

/*
 * CAN ID 路由表 —— 用户按需修改
 *
 * 格式: { CAN ID, 处理函数 }
 * 匹配规则: 精确匹配 CAN ID
 */
static const struct
{
    uint32_t           can_id;
    can_bus_handler_t  handler;
} s_can_bus_table[] = 
{
    /* 示例条目 —— 用户根据实际 CAN 网络填入 */
    /* { 0x100,  app_display_normal_can_handler }, */
    /* { 0x101,  app_display_normal_can_handler }, */
    /* { UDS_REQUEST_CAN_ID,  uds_feed_can_message            }, */
};

/****************************************************************************/
/*                         Static Functions                                 */
/****************************************************************************/

static void can_bus_dispatch(uint32_t id, const uint8_t *data, uint8_t len)
{
    size_t i;
    size_t count;

    count = sizeof(s_can_bus_table) / sizeof(s_can_bus_table[0]);

    for (i = 0; i < count; i++)
    {
        if (s_can_bus_table[i].can_id == id)
        {
            s_can_bus_table[i].handler(id, data, len);
            return;
        }
    }
}

/****************************************************************************/
/*                         Exported Functions                               */
/****************************************************************************/

void can_bus_init(void)
{
    g_can_driver = can_bus_port_get_driver();
    g_can_driver->init();
    uds_init(UDS_REQUEST_CAN_ID, UDS_RESPONSE_CAN_ID);
    isotp_init_with_driver(g_can_driver);
}


void can_bus_task(void *arg)
{
    uint32_t id;
    uint8_t data[8];
    uint8_t len;

    (void)arg;

    while (1)
    {
        while (g_can_driver->receive(&id, data, &len))
        {
            can_bus_dispatch(id, data, len);
        }
    }
}


int can_bus_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    return g_can_driver->send(id, data, len);
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
