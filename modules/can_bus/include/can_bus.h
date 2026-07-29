#ifndef __CAN_BUS_H_
#define __CAN_BUS_H_

/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef void (*can_bus_handler_t)(uint32_t id, const uint8_t *data, uint8_t len);

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

void can_bus_init(void);
void can_bus_task(void *arg);
int  can_bus_send(uint32_t id, const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
