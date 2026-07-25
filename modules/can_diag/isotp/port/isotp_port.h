#ifndef __ISOTP_PORT_H_
#define __ISOTP_PORT_H_

#include "isotp.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*						Exported Functions								*/
/****************************************************************************/

const isotp_port_driver_t *isotp_port_get_driver(void);

#ifdef __cplusplus
}
#endif

#endif