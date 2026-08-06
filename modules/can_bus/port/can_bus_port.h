#ifndef __CAN_BUS_PORT_H_
#define __CAN_BUS_PORT_H_

/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "can_bus.h"

typedef struct can_port_driver
{
    int      (*init)(void);
    int      (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    int      (*receive)(uint32_t *id, uint8_t *data, uint8_t *len);
    uint32_t (*get_ms)(void);
    void     (*debug)(const char *message, ...);
} can_port_driver_t;
/****************************************************************************/
/*                         Exported Functions                               */
/****************************************************************************/

const can_port_driver_t *can_bus_port_get_driver(void);

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
