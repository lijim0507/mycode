

#ifndef __BLE_H_
#define __BLE_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define BLE_GAP_APPEARANCE_GENERIC_TAG   0x0200
#define BLE_GAP_URI_PREFIX_HTTPS         0x17
#define BLE_GAP_LE_ROLE_PERIPHERAL       0x00

#define DEVICE_NAME "MY_BLE_DEVICE"

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct ble_driver {
    int  (*init)(void *config);
    int  (*deinit)(void);
    void (*run)(void);
    int  (*adv_start)(void *adv_cfg);
    int  (*svc_init)(void *svc_table, void *access_cb, void *register_cb,
                     void *subscribe_cb);
    int  (*notify)(uint16_t conn_handle, uint16_t attr_handle,
                   const uint8_t *data, uint16_t len);
} ble_driver_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

int  ble_init(const ble_driver_t *driver, void *port_cfg);
int  ble_deinit(void);
void ble_task(void *param);
int  ble_notify(uint8_t svc_id, const uint8_t *data, uint16_t len);
int  ble_send(uint8_t channel, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
