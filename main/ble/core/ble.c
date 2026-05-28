/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ble.h"
#include "ble_port.h"

#include "common.h"
#include "app_protocol.h"
#include "esp_log.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define TAG "BLE"
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static int  ble_access_callback(uint16_t conn_handle, uint16_t attr_handle,
                                void *ctxt, void *arg);
static void ble_register_cb(void *ctxt, void *arg);
static void ble_subscribe_cb(void *event);

static void ble_svc1_chr1_on_write(const uint8_t *data, uint16_t len);
static int  ble_svc1_chr1_send_notify(const uint8_t *data, uint16_t len);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const ble_driver_t *g_driver;
static bool                g_initialized;

/* 服务1 特征值1 连接状态 */
static uint16_t g_svc1_chr1_val_handle;
static uint16_t g_svc1_chr1_conn_handle;
static bool     g_svc1_chr1_conn_inited;
static bool     g_svc1_chr1_notify_enabled;

static uint8_t g_svc1_chr1_tx_buf[64];
static uint8_t g_svc1_chr1_rx_buf[64];
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int ble_init(const ble_driver_t *driver, void *port_cfg)
{
    int rc;

    if (!driver || !driver->init || !driver->svc_init) {
        return -1;
    }

    if (g_initialized) {
        ble_deinit();
    }

    rc = driver->init(port_cfg);
    if (rc != 0) {
        ESP_LOGE(TAG, "driver init failed, rc=%d", rc);
        return -2;
    }

    rc = driver->svc_init(NULL, (void *)ble_access_callback,
                          (void *)ble_register_cb, (void *)ble_subscribe_cb);
    if (rc != 0) {
        ESP_LOGE(TAG, "svc init failed, rc=%d", rc);
        driver->deinit();
        return -3;
    }

    g_driver      = driver;
    g_initialized = true;
    return 0;
}

int ble_deinit(void)
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

void ble_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLE host task started");

    if (g_driver && g_driver->run) {
        g_driver->run();
    }

    vTaskDelete(NULL);
}

int ble_notify(uint8_t svc_id, const uint8_t *data, uint16_t len)
{
    if (!g_initialized || !g_driver) {
        return -1;
    }

    if (svc_id == 1) {
        return ble_svc1_chr1_send_notify(data, len);
    }

    return -2;
}

int ble_send(uint8_t channel, const uint8_t *data, uint16_t len)
{
    (void)channel;
    return ble_notify(1, data, len);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int ble_access_callback(uint16_t conn_handle, uint16_t attr_handle,
                                void *ctxt, void *arg)
{
    (void)conn_handle;
    (void)arg;

    int      op      = *(int *)ctxt;
    uint16_t data_len;

    switch (op) {
    case 0: /* READ */
        if (attr_handle == g_svc1_chr1_val_handle) {
            ESP_LOGI(TAG, "svc1 chr1 read");
        }
        break;
    case 1: /* WRITE */
        /* data processing delegated to port layer with callback */
        break;
    default:
        break;
    }

    return 0;
}

static void ble_register_cb(void *ctxt, void *arg)
{
    (void)ctxt;
    (void)arg;
}

static void ble_subscribe_cb(void *event)
{
    (void)event;
}

static void ble_svc1_chr1_on_write(const uint8_t *data, uint16_t len)
{
    if (data && len > 0 && len <= sizeof(g_svc1_chr1_rx_buf)) {
        memcpy(g_svc1_chr1_rx_buf, data, len);
        app_protocol_parse(NULL, data, len);
    }
}

static int ble_svc1_chr1_send_notify(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0 || len > sizeof(g_svc1_chr1_tx_buf)) {
        ESP_LOGE(TAG, "svc1 chr1 notify data invalid, len=%d", len);
        return -1;
    }

    if (!g_svc1_chr1_notify_enabled || !g_svc1_chr1_conn_inited) {
        ESP_LOGE(TAG, "svc1 chr1 notify not ready");
        return -2;
    }

    memcpy(g_svc1_chr1_tx_buf, data, len);
    return g_driver->notify(g_svc1_chr1_conn_handle,
                             g_svc1_chr1_val_handle, data, len);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
