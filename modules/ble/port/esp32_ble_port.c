/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ble_port.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "store/config/ble_store_config.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define TAG "BLE_PORT"

#define ESP32_BLE_UUID128_LEN       16

/* 单次写入可接收的最大字节数 */
#ifndef ESP32_BLE_CHR_WRITE_MAX
#define ESP32_BLE_CHR_WRITE_MAX     64
#endif

/* ATT 头部开销，分片时从 MTU 中扣除 */
#define ESP32_BLE_ATT_HEADER_LEN    3
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/* core 送来的一次性回调，保存在 port 里供 NimBLE 事件分发时使用 */
typedef struct {
    ble_subscribe_cb_t subscribe_cb;
    ble_conn_cb_t      conn_cb;
} ble_cb_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  esp32_ble_init(void *config);
static int  esp32_ble_deinit(void);
static int  esp32_ble_adv_start(void *adv_cfg);
static int  esp32_ble_svc_init(const ble_svc_table_t *table,
                                ble_subscribe_cb_t subscribe_cb,
                                ble_conn_cb_t conn_cb);
static int  esp32_ble_notify(uint16_t conn_handle, uint16_t attr_handle,
                              const uint8_t *data, uint16_t len);

static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void ble_host_config_init(void);
static int  gap_init(void);
static int  gap_event_handler(struct ble_gap_event *event, void *arg);
static int  gatt_svc_init(const ble_svc_table_t *table);
static int  gatt_table_build(const ble_svc_table_t *table);
static int  gatt_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
static ble_gatt_chr_flags gatt_convert_flags(uint16_t flags);

static void format_addr(char *addr_str, uint8_t addr[]);
static void print_conn_desc(struct ble_gap_conn_desc *desc);
static void gap_start_advertising(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static ble_cb_ctx_t g_ble_cbs;

static SemaphoreHandle_t g_notify_mutex;

static uint8_t own_addr_type;
static uint8_t addr_val[6];
static uint8_t esp_uri[] = {BLE_GAP_URI_PREFIX_HTTPS, '/', '/', 'e', 's', 'p', 'r', 'e', 's', 's', 'i', 'f', '.', 'c', 'o', 'm'};

/* GATT 描述表由已注册服务在运行期构建。
   NimBLE 保存的是这些结构的指针而不是副本，所以不能放栈上，必须保持静态存储期。 */
static ble_uuid128_t            g_svc_uuid[BLE_SVC_MAX];
static ble_uuid128_t            g_chr_uuid[BLE_SVC_MAX][BLE_SVC_MAX_CHR];
static struct ble_gatt_chr_def  g_chr_defs[BLE_SVC_MAX][BLE_SVC_MAX_CHR + 1];
static struct ble_gatt_svc_def  g_svc_defs[BLE_SVC_MAX + 1];
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const ble_driver_t *ble_port_get_driver(void)
{
    static const ble_driver_t driver = {
        .init       = esp32_ble_init,
        .deinit     = esp32_ble_deinit,
        .run        = nimble_port_run,
        .adv_start  = esp32_ble_adv_start,
        .svc_init   = esp32_ble_svc_init,
        .notify     = esp32_ble_notify,
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

    if (!g_notify_mutex) {
        g_notify_mutex = xSemaphoreCreateMutex();
        if (!g_notify_mutex) {
            return -1;
        }
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

/**
 * @brief  初始化 GAP/GATT 并把服务表注册进协议栈。
 *
 * @param  table        core 维护的服务表，不能为空。
 * @param  subscribe_cb 订阅状态回调，可为 NULL。
 * @param  conn_cb      连接状态回调，可为 NULL。
 * @return 0 成功, -1 服务表无效, 其它为 NimBLE 返回的错误码。
 */
static int esp32_ble_svc_init(const ble_svc_table_t *table,
                               ble_subscribe_cb_t subscribe_cb,
                               ble_conn_cb_t conn_cb)
{
    int rc;

    g_ble_cbs.subscribe_cb = subscribe_cb;
    g_ble_cbs.conn_cb      = conn_cb;

    if (!table || !table->svcs || table->count == 0 ||
        table->count > BLE_SVC_MAX) {
        ESP_LOGE(TAG, "svc table invalid");
        return -1;
    }

    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "gap_init failed, rc=%d", rc);
        return rc;
    }

    rc = gatt_svc_init(table);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatt_svc_init failed, rc=%d", rc);
        return rc;
    }

    ble_host_config_init();
    return 0;
}

/**
 * @brief  把一条消息按当前 MTU 分片后逐片通知出去。
 *
 * mutex 包住的是整个分片循环而不是单次 notify：一旦两个任务的分片交替，
 * 对端收到的就是一串无法还原的字节流。锁放在 port 而不是 core，
 * 是因为这里才是唯一知道"一条消息由哪些片组成"的地方。
 * chunk_max 取 ble_att_mtu() - 3，扣除 ATT 头的 opcode 与 handle 字段。
 *
 * @param  conn_handle 目标连接句柄。
 * @param  attr_handle 目标特征的 value handle。
 * @param  data        待发送数据，不能为空。
 * @param  len         数据长度，不能为 0。
 * @return 0 全部发送成功, -1 参数错误、MTU 异常、mbuf 分配失败或 notify 失败。
 */
static int esp32_ble_notify(uint16_t conn_handle, uint16_t attr_handle,
                                 const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0;
    uint16_t chunk_max;
    int      rc = 0;
    bool     locked = false;

    if (!data || len == 0) {
        return -1;
    }

    /* 整行日志的分片必须连续，否则多任务同时写会把分片交错 */
    if (g_notify_mutex) {
        if (xSemaphoreTake(g_notify_mutex, portMAX_DELAY) != pdTRUE) {
            return -1;
        }
        locked = true;
    }

    chunk_max = ble_att_mtu(conn_handle);
    if (chunk_max <= ESP32_BLE_ATT_HEADER_LEN) {
        rc = -1;
    } else {
        chunk_max -= ESP32_BLE_ATT_HEADER_LEN;
    }

    while (rc == 0 && offset < len) {
        uint16_t       chunk_len = (len - offset) > chunk_max ? chunk_max : (len - offset);
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset, chunk_len);

        if (!om) {
            rc = -1;
            break;
        }

        if (ble_gatts_notify_custom(conn_handle, attr_handle, om) != 0) {
            rc = -1;
            break;
        }

        offset += chunk_len;
    }

    if (locked) {
        xSemaphoreGive(g_notify_mutex);
    }

    return rc;
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

            if (g_ble_cbs.conn_cb) {
                g_ble_cbs.conn_cb(event->connect.conn_handle, true);
            }
        } else {
            gap_start_advertising();
        }
        return rc;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected, reason=%d", event->disconnect.reason);
        if (g_ble_cbs.conn_cb) {
            g_ble_cbs.conn_cb(event->disconnect.conn.conn_handle, false);
        }
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
            g_ble_cbs.subscribe_cb(event->subscribe.conn_handle,
                                   event->subscribe.attr_handle,
                                   event->subscribe.cur_notify != 0);
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

/**
 * @brief  构建 GATT 描述表并提交给 NimBLE 注册。
 *
 * @param  table core 维护的服务表。
 * @return 0 成功, 其它为 NimBLE 返回的错误码。
 * @note   count_cfg 与 add_svcs 必须成对调用，且只在 host 初始化阶段执行一次。
 */
static int gatt_svc_init(const ble_svc_table_t *table)
{
    int rc;

    ble_svc_gatt_init();

    rc = gatt_table_build(table);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatt table build failed, rc=%d", rc);
        return rc;
    }

    rc = ble_gatts_count_cfg(g_svc_defs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(g_svc_defs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * @brief  把 core 的平台无关服务表翻译成 NimBLE 的 ble_gatt_svc_def[]。
 *
 * uuid 只做 16 字节拷贝，flags 逐位翻译，chr->arg 直接指向原始的
 * ble_svc_chr_t。访问回调顺着 arg 就能找回调用方注册的回调与 val_handle，
 * 不需要额外的查找表。
 *
 * @param  table core 维护的服务表，不能为空。
 * @return 0 成功, -1 某个服务描述不完整。
 * @note   输出写入静态数组，NimBLE 保存的是指针而非副本，不能放栈上。
 */
static int gatt_table_build(const ble_svc_table_t *table)
{
    uint8_t svc_index;

    for (svc_index = 0; svc_index < table->count; svc_index++) {
        const ble_svc_def_t *svc = table->svcs[svc_index];
        uint8_t              chr_index;

        if (!svc || !svc->chrs || svc->chr_num == 0 ||
            svc->chr_num > BLE_SVC_MAX_CHR) {
            return -1;
        }

        g_svc_uuid[svc_index].u.type = BLE_UUID_TYPE_128;
        memcpy(g_svc_uuid[svc_index].value, svc->uuid.bytes, ESP32_BLE_UUID128_LEN);

        for (chr_index = 0; chr_index < svc->chr_num; chr_index++) {
            ble_svc_chr_t *chr = &svc->chrs[chr_index];

            g_chr_uuid[svc_index][chr_index].u.type = BLE_UUID_TYPE_128;
            memcpy(g_chr_uuid[svc_index][chr_index].value, chr->uuid.bytes,
                   ESP32_BLE_UUID128_LEN);

            g_chr_defs[svc_index][chr_index].uuid       = &g_chr_uuid[svc_index][chr_index].u;
            g_chr_defs[svc_index][chr_index].access_cb  = gatt_chr_access_cb;
            g_chr_defs[svc_index][chr_index].arg        = chr;
            g_chr_defs[svc_index][chr_index].flags      = gatt_convert_flags(chr->flags);
            g_chr_defs[svc_index][chr_index].val_handle = chr->val_handle;
        }

        g_svc_defs[svc_index].type            = BLE_GATT_SVC_TYPE_PRIMARY;
        g_svc_defs[svc_index].uuid            = &g_svc_uuid[svc_index].u;
        g_svc_defs[svc_index].characteristics = g_chr_defs[svc_index];
    }

    return 0;
}

/**
 * @brief  把 BLE_SVC_CHR_FLAG_xxx 抽象位翻译成 NimBLE 原生 flag。
 *
 * @param  flags BLE_SVC_CHR_FLAG_xxx 的按位或。
 * @return 对应的 ble_gatt_chr_flags 组合。
 */
static ble_gatt_chr_flags gatt_convert_flags(uint16_t flags)
{
    ble_gatt_chr_flags out = 0;

    if (flags & BLE_SVC_CHR_FLAG_READ) {
        out |= BLE_GATT_CHR_F_READ;
    }
    if (flags & BLE_SVC_CHR_FLAG_WRITE) {
        out |= BLE_GATT_CHR_F_WRITE;
    }
    if (flags & BLE_SVC_CHR_FLAG_WRITE_NO_RSP) {
        out |= BLE_GATT_CHR_F_WRITE_NO_RSP;
    }
    if (flags & BLE_SVC_CHR_FLAG_NOTIFY) {
        out |= BLE_GATT_CHR_F_NOTIFY;
    }
    if (flags & BLE_SVC_CHR_FLAG_INDICATE) {
        out |= BLE_GATT_CHR_F_INDICATE;
    }

    return out;
}

/**
 * @brief  所有特征共用的访问回调，通过 arg 区分是谁。
 *
 * arg 是 gatt_table_build() 里填进去的 ble_svc_chr_t 指针，
 * 因此这里能拿到调用方注册的 access_cb 和 val_handle。
 *
 * @param  conn_handle 发起访问的连接句柄。
 * @param  attr_handle 被访问的特征 value handle。
 * @param  ctxt        NimBLE 访问上下文，含 op 与数据 mbuf。
 * @param  arg         指向该特征的 ble_svc_chr_t。
 * @return 0 成功, 否则为 ATT 错误码。
 * @note   写入数据从 mbuf 拷到栈缓冲，回调返回后即失效，调用方要留存必须自行拷贝。
 */
static int gatt_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ble_svc_chr_t *chr = (ble_svc_chr_t *)arg;
    uint8_t        recv_buf[ESP32_BLE_CHR_WRITE_MAX];
    uint16_t       data_len;

    if (!chr) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* 本抽象只覆盖写入与通知，读请求返回空值 */
        if (chr->access_cb) {
            chr->access_cb(conn_handle, attr_handle, BLE_SVC_ACCESS_READ, NULL, 0);
        }
        return 0;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        data_len = OS_MBUF_PKTLEN(ctxt->om);
        if (data_len == 0 || data_len > sizeof(recv_buf)) {
            ESP_LOGE(TAG, "write data len invalid: %d", data_len);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        if (os_mbuf_copydata(ctxt->om, 0, data_len, recv_buf) != 0) {
            ESP_LOGE(TAG, "mbuf copydata fail");
            return BLE_ATT_ERR_UNLIKELY;
        }

        if (chr->access_cb) {
            chr->access_cb(conn_handle, attr_handle, BLE_SVC_ACCESS_WRITE,
                           recv_buf, data_len);
        }
        return 0;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
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
        ESP_LOGD(TAG, "registered chr %s val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.val_handle);
        break;
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registered dsc %s handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;
    default:
        break;
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
