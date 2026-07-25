/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ble_port.h"

#include <string.h>

#include "common.h"
#include "esp_log.h"

#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define TAG "BLE_PORT"
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef void (*ble_access_cb_t)(uint16_t conn_handle, uint16_t attr_handle,
                                 void *ctxt, void *arg);
typedef void (*ble_register_cb_t)(void *ctxt, void *arg);
typedef void (*ble_subscribe_cb_t)(void *event);

typedef struct {
    ble_access_cb_t   access_cb;
    ble_register_cb_t register_cb;
    ble_subscribe_cb_t subscribe_cb;
} ble_cb_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  esp32_ble_init(void *config);
static int  esp32_ble_deinit(void);
static int  esp32_ble_adv_start(void *adv_cfg);
static int  esp32_ble_svc_init(void *svc_table, void *access_cb,
                                void *register_cb, void *subscribe_cb);
static int  esp32_ble_notify(uint16_t conn_handle, uint16_t attr_handle,
                              const uint8_t *data, uint16_t len);

static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void ble_host_config_init(void);
static int  gap_event_handler(struct ble_gap_event *event, void *arg);
static int  gatt_svr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

static void format_addr(char *addr_str, uint8_t addr[]);
static void print_conn_desc(struct ble_gap_conn_desc *desc);
static void gap_start_advertising(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static ble_cb_ctx_t g_ble_cbs;

static uint8_t own_addr_type;
static uint8_t addr_val[6];
static uint8_t esp_uri[] = {BLE_GAP_URI_PREFIX_HTTPS, '/', '/', 'e', 's', 'p', 'r', 'e', 's', 's', 'i', 'f', '.', 'c', 'o', 'm'};

/* GATT 服务 UUID */
#define SVC_UUID(...) BLE_UUID128_INIT(__VA_ARGS__)
#define CHR_UUID(...) BLE_UUID128_INIT(__VA_ARGS__)

static const ble_uuid128_t gatt_svr_svc1_uuid =
    SVC_UUID(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10);

static const ble_uuid128_t gatt_svr_chr1_uuid =
    CHR_UUID(0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20);

static uint16_t gatt_svr_chr1_val_handle;

static const ble_uuid128_t gatt_svr_chr2_uuid =
    CHR_UUID(0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
             0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30);

static uint16_t gatt_svr_chr2_val_handle;

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc1_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gatt_svr_chr1_uuid.u,
                .access_cb = gatt_svr_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &gatt_svr_chr1_val_handle,
            },
            {
                .uuid = &gatt_svr_chr2_uuid.u,
                .access_cb = gatt_svr_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                .val_handle = &gatt_svr_chr2_val_handle,
            },
            { 0 },
        },
    },
    { 0 },
};
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const ble_driver_t *ble_port_get_driver(void)
{
    static const ble_driver_t driver = {
        .init      = esp32_ble_init,
        .deinit    = esp32_ble_deinit,
        .run       = nimble_port_run,
        .adv_start = esp32_ble_adv_start,
        .svc_init  = esp32_ble_svc_init,
        .notify    = esp32_ble_notify,
    };
    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int esp32_ble_init(void *config)
{
    (void)config;

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        return -1;
    }
    return 0;
}

static int esp32_ble_deinit(void)
{
    nimble_port_stop();
    return 0;
}

static int esp32_ble_adv_start(void *adv_cfg)
{
    (void)adv_cfg;
    gap_start_advertising();
    return 0;
}

static int esp32_ble_svc_init(void *svc_table, void *access_cb,
                               void *register_cb, void *subscribe_cb)
{
    (void)svc_table;

    g_ble_cbs.access_cb    = (ble_access_cb_t)access_cb;
    g_ble_cbs.register_cb  = (ble_register_cb_t)register_cb;
    g_ble_cbs.subscribe_cb = (ble_subscribe_cb_t)subscribe_cb;

    int rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "gap_init failed, rc=%d", rc);
        return rc;
    }

    rc = gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "gatt_svc_init failed, rc=%d", rc);
        return rc;
    }

    ble_host_config_init();
    return 0;
}

static int esp32_ble_notify(uint16_t conn_handle, uint16_t attr_handle,
                             const uint8_t *data, uint16_t len)
{
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) {
        return -1;
    }

    return ble_gatts_notify_custom(conn_handle, attr_handle, om);
}

/* --- NimBLE 回调 --- */

static void on_stack_reset(int reason)
{
    ESP_LOGI(TAG, "nimble stack reset, reason=%d", reason);
}

static void on_stack_sync(void)
{
    gap_start_advertising();
}

static void ble_host_config_init(void)
{
    ble_hs_cfg.reset_cb         = on_stack_reset;
    ble_hs_cfg.sync_cb          = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb   = NULL;
    ble_store_config_init();
}

/* --- GAP --- */

static int gap_init(void)
{
    int rc;

    ble_svc_gap_init();

    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "set device name failed, rc=%d", rc);
    }
    return rc;
}

static void format_addr(char *addr_str, uint8_t addr[])
{
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void print_conn_desc(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(TAG, "conn: itvl=%d latency=%d timeout=%d",
             desc->conn_itvl, desc->latency, desc->supervision_timeout);
}

static void gap_start_advertising(void)
{
    int rc;
    char addr_str[18] = {0};
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "no bt address available");
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer addr type failed, rc=%d", rc);
        return;
    }

    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "copy addr failed, rc=%d", rc);
        return;
    }
    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "device addr: %s", addr_str);

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "set adv data failed, rc=%d", rc);
        return;
    }

    rsp_fields.device_addr = addr_val;
    rsp_fields.device_addr_type = own_addr_type;
    rsp_fields.device_addr_is_present = 1;
    rsp_fields.uri = esp_uri;
    rsp_fields.uri_len = sizeof(esp_uri);
    rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(500);
    rsp_fields.adv_itvl_is_present = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "set scan rsp failed, rc=%d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "start adv failed, rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started");
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    int rc = 0;
    struct ble_gap_conn_desc desc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);

        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc != 0) {
                ESP_LOGE(TAG, "conn find failed, rc=%d", rc);
                return rc;
            }
            print_conn_desc(&desc);

            struct ble_gap_upd_params params = {
                .itvl_min = desc.conn_itvl,
                .itvl_max = desc.conn_itvl,
                .latency = 3,
                .supervision_timeout = desc.supervision_timeout,
            };
            rc = ble_gap_update_params(event->connect.conn_handle, &params);
            if (rc != 0) {
                ESP_LOGE(TAG, "update params failed, rc=%d", rc);
            }
        } else {
            gap_start_advertising();
        }
        return rc;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected, reason=%d", event->disconnect.reason);
        gap_start_advertising();
        return rc;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "conn updated, status=%d", event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        if (rc != 0) {
            return rc;
        }
        print_conn_desc(&desc);
        return rc;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "adv complete, reason=%d", event->adv_complete.reason);
        gap_start_advertising();
        return rc;

    case BLE_GAP_EVENT_NOTIFY_TX:
        if (event->notify_tx.status != 0 &&
            event->notify_tx.status != BLE_HS_EDONE) {
            ESP_LOGI(TAG, "notify event; conn=%d attr=%d status=%d",
                     event->notify_tx.conn_handle,
                     event->notify_tx.attr_handle,
                     event->notify_tx.status);
        }
        return rc;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe; conn=%d attr=%d notify=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle,
                 event->subscribe.cur_notify);
        if (g_ble_cbs.subscribe_cb) {
            g_ble_cbs.subscribe_cb(event);
        }
        return rc;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update; conn=%d cid=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.channel_id,
                 event->mtu.value);
        return rc;
    }

    return rc;
}

/* --- GATT --- */

static int gatt_svc_init(void)
{
    int rc;

    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int gatt_svr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)arg;
    int rc = 0;
    uint16_t data_len;
    uint8_t recv_buf[64] = {0};

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == gatt_svr_chr1_val_handle) {
            ESP_LOGI(TAG, "svc1 chr1 read");
        } else if (attr_handle == gatt_svr_chr2_val_handle) {
            ESP_LOGI(TAG, "svc1 chr2 read");
        }
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        data_len = OS_MBUF_PKTLEN(ctxt->om);
        if (data_len == 0 || data_len > sizeof(recv_buf)) {
            ESP_LOGE(TAG, "write data len invalid: %d", data_len);
            return BLE_ATT_ERR_UNLIKELY;
        }

        rc = os_mbuf_copydata(ctxt->om, 0, data_len, recv_buf);
        if (rc != 0) {
            ESP_LOGE(TAG, "mbuf copydata fail, rc=%d", rc);
            return BLE_ATT_ERR_UNLIKELY;
        }

        if (attr_handle == gatt_svr_chr1_val_handle) {
            ESP_LOGI(TAG, "svc1 chr1 write, len=%d", data_len);
            if (g_ble_cbs.access_cb) {
                int op = 1;
                g_ble_cbs.access_cb(conn_handle, attr_handle, &op, recv_buf);
            }
        } else if (attr_handle == gatt_svr_chr2_val_handle) {
            ESP_LOGI(TAG, "svc1 chr2 write, len=%d", data_len);
        }
        break;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

static void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    (void)arg;
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered svc %s handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registering chr %s val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.val_handle);
        break;
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering dsc %s handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;
    default:
        break;
    }

    if (g_ble_cbs.register_cb) {
        g_ble_cbs.register_cb(ctxt, arg);
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
