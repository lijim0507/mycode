#ifndef __CAN_BUS_PORT_H_
#define __CAN_BUS_PORT_H_

/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "can_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  CAN 总线抽象接口（符合 can_port_driver_t 标准）
 */
typedef struct
{
    int  (*init)(void);
    int  (*deinit)(void);
    int  (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    int  (*receive)(uint32_t *id, uint8_t *data, uint8_t *len, uint32_t timeout_ms);
} can_port_driver_t;

/****************************************************************************/
/*                         Exported Functions                               */
/****************************************************************************/

const can_port_driver_t *can_bus_port_get_driver(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
