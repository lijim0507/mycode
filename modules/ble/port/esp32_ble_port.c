/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "ble_port.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "nvs_flash.h"

#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"

/* NimBLE 的 store 配置初始化没有公开声明，按官方示例自行声明 */
void ble_store_config_init(void);

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/
#define TAG "BLE_PORT"

#define ESP32_BLE_UUID128_LEN       16

/* ATT 头部开销，发送分片和读取响应时从 MTU 中扣除 */
#define ESP32_BLE_ATT_HEADER_LEN    3

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/* core 送来的一次性回调，保存在 port 里供 NimBLE 事件分发时使用 */
typedef struct
{
    ble_subscribe_cb_t subscribe_cb;
    ble_conn_cb_t      conn_cb;
} esp32_ble_cb_ctx_t;

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  esp32_ble_init(void);
static int  esp32_ble_deinit(void);
static void esp32_ble_run(void);
static int  esp32_ble_adv_start(void);
static int  esp32_ble_adv_stop(void);
static int  esp32_ble_svc_init(const ble_svc_table_t *table,
                               ble_subscribe_cb_t subscribe_cb,
                               ble_conn_cb_t conn_cb);
static int  esp32_ble_notify(uint16_t conn_handle, uint16_t attr_handle,
                             const uint8_t *data, uint16_t len);
static int  esp32_ble_indicate(uint16_t conn_handle, uint16_t attr_handle,
                               const uint8_t *data, uint16_t len);

static void esp32_ble_on_reset(int reason);
static void esp32_ble_on_sync(void);
static void esp32_ble_host_config_init(void);
static int  esp32_ble_gap_init(void);
static int  esp32_ble_gap_advertising_start(void);
static void esp32_ble_gap_auto_restart(void);
static int  esp32_ble_gap_event(struct ble_gap_event *event, void *arg);
static int  esp32_ble_gatt_init(const ble_svc_table_t *table);
static int  esp32_ble_gatt_table_build(const ble_svc_table_t *table);
static int  esp32_ble_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);
static void esp32_ble_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt,
                                       void *arg);
static ble_gatt_chr_flags esp32_ble_convert_flags(uint16_t flags);
static int  esp32_ble_send(uint16_t conn_handle, uint16_t attr_handle,
                           const uint8_t *data, uint16_t len, bool indicate);
static void esp32_ble_format_addr(char *addr_str, uint8_t addr[]);
static void esp32_ble_print_conn_desc(struct ble_gap_conn_desc *desc);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static esp32_ble_cb_ctx_t g_cbs;

static SemaphoreHandle_t g_notify_mutex;

/* 宿主任务是否已经跑起来，port deinit 据此决定要不要先 nimble_port_stop()。
   由宿主任务自己写、由调用 deinit 的任务读，只用于判断，不加锁。 */
static volatile bool g_host_started;

/* 手动 stop 广播后置位，抑制断开/广播结束时的自动重启 */
static bool g_adv_suppressed;

static uint8_t g_own_addr_type;
static uint8_t g_addr_val[6];

/* GATT 描述表由已注册服务在运行期构建。
   NimBLE 保存的是这些结构的指针而不是副本，所以不能放栈上，必须保持静态存储期。 */
static ble_uuid16_t             g_svc_uuid16[BLE_SVC_MAX];
static ble_uuid128_t            g_svc_uuid128[BLE_SVC_MAX];
static ble_uuid16_t             g_chr_uuid16[BLE_SVC_MAX][BLE_SVC_MAX_CHR];
static ble_uuid128_t            g_chr_uuid128[BLE_SVC_MAX][BLE_SVC_MAX_CHR];
static struct ble_gatt_chr_def  g_chr_defs[BLE_SVC_MAX][BLE_SVC_MAX_CHR + 1];
static struct ble_gatt_svc_def  g_svc_defs[BLE_SVC_MAX + 1];

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 ESP32 BLE 驱动实例
 *
 * @return 驱动实例指针
 */
const ble_driver_t *ble_port_get_driver(void)
{
    static const ble_driver_t driver = {
        .init      = esp32_ble_init,
        .deinit    = esp32_ble_deinit,
        .run       = esp32_ble_run,
        .adv_start = esp32_ble_adv_start,
        .adv_stop  = esp32_ble_adv_stop,
        .svc_init  = esp32_ble_svc_init,
        .notify    = esp32_ble_notify,
        .indicate  = esp32_ble_indicate,
    };

    return &driver;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 NVS 和 NimBLE 协议栈
 *
 * @return 0: 成功, -1: NVS 初始化失败, -2: 协议栈初始化失败, -3: 互斥量创建失败
 */
static int esp32_ble_init(void)
{
    esp_err_t ret;

    /* BLE 协议栈依赖 NVS 保存配置，应用已经初始化过时重复调用也不会出错 */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "nvs init failed (%d), erase and retry", ret);

        ret = nvs_flash_erase();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "nvs erase failed: %d", ret);
            return -1;
        }

        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs init failed: %d", ret);
        return -1;
    }

    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "nimble init failed: %d", ret);
        return -2;
    }

    if (!g_notify_mutex)
    {
        g_notify_mutex = xSemaphoreCreateMutex();
        if (!g_notify_mutex)
        {
            ESP_LOGE(TAG, "create notify mutex failed");
            nimble_port_deinit();
            return -3;
        }
    }

    g_host_started = false;
    g_adv_suppressed = false;
    return 0;
}

/**
 * @brief  停止协议栈并释放资源
 *
 * 宿主任务已经跑起来时先 nimble_port_stop() 等它退出，再 deinit；
 * 宿主任务从未启动（例如 svc_init 失败）时直接 deinit，避免 stop 永远等不到事件。
 *
 * @return 0: 成功, -1: 停止或反初始化失败
 */
static int esp32_ble_deinit(void)
{
    esp_err_t ret;
    int rc;

    if (g_host_started)
    {
        rc = nimble_port_stop();
        if (rc != 0)
        {
            ESP_LOGE(TAG, "nimble stop failed: %d", rc);
            return -1;
        }
    }

    ret = nimble_port_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "nimble deinit failed: %d", ret);
        return -1;
    }

    if (g_notify_mutex)
    {
        vSemaphoreDelete(g_notify_mutex);
        g_notify_mutex = NULL;
    }

    g_host_started = false;
    g_adv_suppressed = false;
    g_cbs.subscribe_cb = NULL;
    g_cbs.conn_cb = NULL;
    return 0;
}

/**
 * @brief  协议栈宿主任务入口，内部进入 nimble_port_run() 后不再返回
 */
static void esp32_ble_run(void)
{
    g_host_started = true;
    nimble_port_run();
    g_host_started = false;
}

/**
 * @brief  手动开始广播，同时解除 stop 造成的自动重启抑制
 *
 * @return 0: 成功, -1: 启动失败
 */
static int esp32_ble_adv_start(void)
{
    g_adv_suppressed = false;
    return esp32_ble_gap_advertising_start();
}

/**
 * @brief  手动停止广播，并抑制后续的自动重启
 *
 * @return 0: 成功, -1: 停止失败
 */
static int esp32_ble_adv_stop(void)
{
    int rc;

    g_adv_suppressed = true;

    rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY)
    {
        ESP_LOGW(TAG, "adv stop failed: %d", rc);
        return -1;
    }

    return 0;
}

/**
 * @brief  初始化 GAP/GATT 并把服务表注册进协议栈
 *
 * @param  table        core 维护的服务表，可以为空。
 * @param  subscribe_cb 订阅状态回调，可为 NULL。
 * @param  conn_cb      连接状态回调，可为 NULL。
 * @return 0 成功, -1 服务表无效, 其它为 NimBLE 返回的错误码。
 */
static int esp32_ble_svc_init(const ble_svc_table_t *table,
                              ble_subscribe_cb_t subscribe_cb,
                              ble_conn_cb_t conn_cb)
{
    int rc;

    if (!table || (!table->svcs && table->count > 0) ||
        table->count > BLE_SVC_MAX)
    {
        ESP_LOGE(TAG, "svc table invalid");
        return -1;
    }

    g_cbs.subscribe_cb = subscribe_cb;
    g_cbs.conn_cb = conn_cb;

    rc = esp32_ble_gap_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "gap init failed, rc=%d", rc);
        return rc;
    }

    rc = esp32_ble_gatt_init(table);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "gatt init failed, rc=%d", rc);
        return rc;
    }

    esp32_ble_host_config_init();
    return 0;
}

/**
 * @brief  发送通知
 *
 * @param  conn_handle 目标连接句柄。
 * @param  attr_handle 目标特征的 value handle。
 * @param  data        待发送数据。
 * @param  len         数据长度。
 * @return 0 成功, -1 参数错误或发送失败。
 */
static int esp32_ble_notify(uint16_t conn_handle, uint16_t attr_handle,
                            const uint8_t *data, uint16_t len)
{
    return esp32_ble_send(conn_handle, attr_handle, data, len, false);
}

/**
 * @brief  发送指示
 *
 * @param  conn_handle 目标连接句柄。
 * @param  attr_handle 目标特征的 value handle。
 * @param  data        待发送数据。
 * @param  len         数据长度，不能超过 MTU - 3。
 * @return 0 成功, -1 参数错误、超长或发送失败。
 */
static int esp32_ble_indicate(uint16_t conn_handle, uint16_t attr_handle,
                              const uint8_t *data, uint16_t len)
{
    return esp32_ble_send(conn_handle, attr_handle, data, len, true);
}

/**
 * @brief  协议栈 reset 回调
 *
 * @param  reason 复位原因。
 */
static void esp32_ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "nimble reset, reason=%d", reason);
}

/**
 * @brief  协议栈与控制器同步完成，按配置自动开始广播
 */
static void esp32_ble_on_sync(void)
{
#if BLE_ADV_AUTO_START
    if (!g_adv_suppressed)
    {
        esp32_ble_gap_advertising_start();
    }
#endif
}

/**
 * @brief  挂上 NimBLE host 的全局回调
 */
static void esp32_ble_host_config_init(void)
{
    ble_hs_cfg.reset_cb = esp32_ble_on_reset;
    ble_hs_cfg.sync_cb = esp32_ble_on_sync;
    ble_hs_cfg.gatts_register_cb = esp32_ble_gatt_register_cb;
    ble_hs_cfg.store_status_cb = NULL;

    ble_store_config_init();
}

/**
 * @brief  初始化 GAP 服务并设置设备名
 *
 * @return 0 成功, 其它为 NimBLE 返回的错误码。
 */
static int esp32_ble_gap_init(void)
{
    int rc;

    ble_svc_gap_init();

    rc = ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "set device name failed, rc=%d", rc);
        return rc;
    }

    return 0;
}

/**
 * @brief  配置并启动广播
 *
 * @return 0 成功, -1 地址或广播参数配置失败。
 */
static int esp32_ble_gap_advertising_start(void)
{
    int rc;
    char addr_str[18] = {0};
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "no bt address available, rc=%d", rc);
        return -1;
    }

    rc = ble_hs_id_infer_auto(0, &g_own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "infer addr type failed, rc=%d", rc);
        return -1;
    }

    rc = ble_hs_id_copy_addr(g_own_addr_type, g_addr_val, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "copy addr failed, rc=%d", rc);
        return -1;
    }

    esp32_ble_format_addr(addr_str, g_addr_val);
    ESP_LOGI(TAG, "device addr: %s", addr_str);

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.appearance = BLE_ADV_APPEARANCE;
    adv_fields.appearance_is_present = 1;
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "set adv data failed, rc=%d", rc);
        return -1;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(BLE_ADV_ITVL_MIN_MS);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(BLE_ADV_ITVL_MAX_MS);

    rc = ble_gap_adv_start(g_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           esp32_ble_gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY)
    {
        ESP_LOGE(TAG, "start adv failed, rc=%d", rc);
        return -1;
    }

    ESP_LOGI(TAG, "advertising started");
    return 0;
}

/**
 * @brief  断开或广播结束时按配置自动重启广播
 */
static void esp32_ble_gap_auto_restart(void)
{
#if BLE_ADV_AUTO_START
    if (!g_adv_suppressed)
    {
        esp32_ble_gap_advertising_start();
    }
#endif
}

/**
 * @brief  GAP 事件回调
 *
 * @param  event NimBLE 事件。
 * @param  arg   未使用。
 * @return 0 已处理。
 */
static int esp32_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    int rc;
    struct ble_gap_conn_desc desc;

    (void)arg;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);

        if (event->connect.status != 0)
        {
            esp32_ble_gap_auto_restart();
            return 0;
        }

        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        if (rc != 0)
        {
            ESP_LOGE(TAG, "conn find failed, rc=%d", rc);
            return 0;
        }
        esp32_ble_print_conn_desc(&desc);

#if BLE_CONN_UPDATE_ENABLE
        {
            struct ble_gap_upd_params params = {
                .itvl_min = desc.conn_itvl,
                .itvl_max = desc.conn_itvl,
                .latency = BLE_CONN_LATENCY,
                .supervision_timeout = desc.supervision_timeout,
            };

            rc = ble_gap_update_params(event->connect.conn_handle, &params);
            if (rc != 0)
            {
                ESP_LOGW(TAG, "update params failed, rc=%d", rc);
            }
        }
#endif

        if (g_cbs.conn_cb)
        {
            g_cbs.conn_cb(event->connect.conn_handle, true);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected, reason=%d", event->disconnect.reason);

        if (g_cbs.conn_cb)
        {
            g_cbs.conn_cb(event->disconnect.conn.conn_handle, false);
        }

        esp32_ble_gap_auto_restart();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "conn updated, status=%d", event->conn_update.status);

        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        if (rc != 0)
        {
            ESP_LOGE(TAG, "conn find failed, rc=%d", rc);
            return 0;
        }
        esp32_ble_print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "adv complete, reason=%d", event->adv_complete.reason);
        esp32_ble_gap_auto_restart();
        return 0;

    case BLE_GAP_EVENT_NOTIFY_TX:
        if (event->notify_tx.status != 0 &&
            event->notify_tx.status != BLE_HS_EDONE)
        {
            ESP_LOGW(TAG, "notify event; conn=%d attr=%d status=%d indicate=%d",
                     event->notify_tx.conn_handle,
                     event->notify_tx.attr_handle,
                     event->notify_tx.status,
                     event->notify_tx.indication);
        }
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe; conn=%d attr=%d notify=%d indicate=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle,
                 event->subscribe.cur_notify, event->subscribe.cur_indicate);

        if (g_cbs.subscribe_cb)
        {
            g_cbs.subscribe_cb(event->subscribe.conn_handle,
                               event->subscribe.attr_handle,
                               event->subscribe.cur_notify != 0,
                               event->subscribe.cur_indicate != 0);
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update; conn=%d cid=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.channel_id,
                 event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

/**
 * @brief  初始化 GATT 服务并把 core 的服务表注册给 NimBLE
 *
 * @param  table core 维护的服务表。
 * @return 0 成功, 其它为 NimBLE 返回的错误码。
 */
static int esp32_ble_gatt_init(const ble_svc_table_t *table)
{
    int rc;

    ble_svc_gatt_init();

    rc = esp32_ble_gatt_table_build(table);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "gatt table build failed, rc=%d", rc);
        return rc;
    }

    rc = ble_gatts_count_cfg(g_svc_defs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "gatt count cfg failed, rc=%d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(g_svc_defs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "gatt add svcs failed, rc=%d", rc);
        return rc;
    }

    return 0;
}

/**
 * @brief  把 core 的平台无关服务表翻译成 NimBLE 的 ble_gatt_svc_def[]
 *
 * uuid 按 16/128 位分别填进静态数组，flags 逐位翻译，chr->arg 直接指向
 * 原始的 ble_svc_chr_t。访问回调顺着 arg 就能找回调用方注册的回调与 val_handle，
 * 不需要额外的查找表。
 *
 * @param  table core 维护的服务表，不能为空。
 * @return 0 成功, -1 某个服务或特征描述不完整。
 * @note   输出写入静态数组，NimBLE 保存的是指针而非副本，不能放栈上。
 */
static int esp32_ble_gatt_table_build(const ble_svc_table_t *table)
{
    uint8_t svc_index;

    memset(g_svc_uuid16, 0, sizeof(g_svc_uuid16));
    memset(g_svc_uuid128, 0, sizeof(g_svc_uuid128));
    memset(g_chr_uuid16, 0, sizeof(g_chr_uuid16));
    memset(g_chr_uuid128, 0, sizeof(g_chr_uuid128));
    memset(g_chr_defs, 0, sizeof(g_chr_defs));
    memset(g_svc_defs, 0, sizeof(g_svc_defs));

    for (svc_index = 0; svc_index < table->count; svc_index++)
    {
        const ble_svc_def_t *svc = table->svcs[svc_index];
        uint8_t              chr_index;

        if (!svc || !svc->chrs || svc->chr_num == 0 ||
            svc->chr_num > BLE_SVC_MAX_CHR ||
            (svc->uuid.len != 2 && svc->uuid.len != 16))
        {
            return -1;
        }

        if (svc->uuid.len == 2)
        {
            g_svc_uuid16[svc_index].u.type = BLE_UUID_TYPE_16;
            g_svc_uuid16[svc_index].value = (uint16_t)svc->uuid.bytes[0] |
                                            ((uint16_t)svc->uuid.bytes[1] << 8);
            g_svc_defs[svc_index].uuid = &g_svc_uuid16[svc_index].u;
        }
        else
        {
            g_svc_uuid128[svc_index].u.type = BLE_UUID_TYPE_128;
            memcpy(g_svc_uuid128[svc_index].value, svc->uuid.bytes,
                   ESP32_BLE_UUID128_LEN);
            g_svc_defs[svc_index].uuid = &g_svc_uuid128[svc_index].u;
        }

        for (chr_index = 0; chr_index < svc->chr_num; chr_index++)
        {
            ble_svc_chr_t *chr = &svc->chrs[chr_index];

            if (chr->uuid.len != 2 && chr->uuid.len != 16)
            {
                return -1;
            }

            if (chr->uuid.len == 2)
            {
                g_chr_uuid16[svc_index][chr_index].u.type = BLE_UUID_TYPE_16;
                g_chr_uuid16[svc_index][chr_index].value =
                    (uint16_t)chr->uuid.bytes[0] |
                    ((uint16_t)chr->uuid.bytes[1] << 8);
                g_chr_defs[svc_index][chr_index].uuid =
                    &g_chr_uuid16[svc_index][chr_index].u;
            }
            else
            {
                g_chr_uuid128[svc_index][chr_index].u.type = BLE_UUID_TYPE_128;
                memcpy(g_chr_uuid128[svc_index][chr_index].value,
                       chr->uuid.bytes, ESP32_BLE_UUID128_LEN);
                g_chr_defs[svc_index][chr_index].uuid =
                    &g_chr_uuid128[svc_index][chr_index].u;
            }

            g_chr_defs[svc_index][chr_index].access_cb = esp32_ble_gatt_access;
            g_chr_defs[svc_index][chr_index].arg = chr;
            g_chr_defs[svc_index][chr_index].flags =
                esp32_ble_convert_flags(chr->flags);
            g_chr_defs[svc_index][chr_index].val_handle = chr->val_handle;
        }

        g_svc_defs[svc_index].type = BLE_GATT_SVC_TYPE_PRIMARY;
        g_svc_defs[svc_index].characteristics = g_chr_defs[svc_index];
    }

    return 0;
}

/**
 * @brief  所有特征共用的访问回调，通过 arg 区分是谁
 *
 * arg 是 gatt_table_build() 里填进去的 ble_svc_chr_t 指针，
 * 因此这里能拿到调用方注册的 read_cb / write_cb 和 val_handle。
 *
 * @param  conn_handle 发起访问的连接句柄。
 * @param  attr_handle 被访问的特征 value handle。
 * @param  ctxt        NimBLE 访问上下文，含 op 与数据 mbuf。
 * @param  arg         指向该特征的 ble_svc_chr_t。
 * @return 0 成功, 否则为 ATT 错误码。
 * @note   写入数据从 mbuf 拷到栈缓冲，回调返回后即失效，调用方要留存必须自行拷贝。
 */
static int esp32_ble_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ble_svc_chr_t *chr = (ble_svc_chr_t *)arg;
    int            rc;

    if (!chr)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
    {
        uint8_t  buf[BLE_CHR_READ_MAX];
        uint16_t max_len;
        uint16_t out_len = 0;

        if (!chr->read_cb)
        {
            return BLE_ATT_ERR_READ_NOT_PERMITTED;
        }

        /* ATT 读响应最多 MTU - 1 字节；MTU 未知（协议栈内部读取）时按缓冲上限处理 */
        max_len = ble_att_mtu(conn_handle);
        if (max_len > 1)
        {
            max_len -= 1;
        }
        else
        {
            max_len = sizeof(buf);
        }
        if (max_len > sizeof(buf))
        {
            max_len = sizeof(buf);
        }

        rc = chr->read_cb(conn_handle, attr_handle, buf, max_len, &out_len);
        if (rc != 0 || out_len > max_len)
        {
            return BLE_ATT_ERR_UNLIKELY;
        }

        rc = os_mbuf_append(ctxt->om, buf, out_len);
        return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
    {
        uint8_t  buf[BLE_CHR_WRITE_MAX];
        uint16_t data_len;

        if (!chr->write_cb)
        {
            return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        }

        data_len = OS_MBUF_PKTLEN(ctxt->om);
        if (data_len == 0 || data_len > sizeof(buf))
        {
            ESP_LOGE(TAG, "write data len invalid: %d", data_len);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        if (os_mbuf_copydata(ctxt->om, 0, data_len, buf) != 0)
        {
            ESP_LOGE(TAG, "mbuf copydata failed");
            return BLE_ATT_ERR_UNLIKELY;
        }

        rc = chr->write_cb(conn_handle, attr_handle, buf, data_len);
        return (rc == 0) ? 0 : BLE_ATT_ERR_UNLIKELY;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/**
 * @brief  把 BLE_SVC_CHR_FLAG_xxx 抽象位翻译成 NimBLE 原生 flag
 *
 * @param  flags BLE_SVC_CHR_FLAG_xxx 的按位或。
 * @return 对应的 ble_gatt_chr_flags 组合。
 */
static ble_gatt_chr_flags esp32_ble_convert_flags(uint16_t flags)
{
    ble_gatt_chr_flags out = 0;

    if (flags & BLE_SVC_CHR_FLAG_READ)
    {
        out |= BLE_GATT_CHR_F_READ;
    }
    if (flags & BLE_SVC_CHR_FLAG_WRITE)
    {
        out |= BLE_GATT_CHR_F_WRITE;
    }
    if (flags & BLE_SVC_CHR_FLAG_WRITE_NO_RSP)
    {
        out |= BLE_GATT_CHR_F_WRITE_NO_RSP;
    }
    if (flags & BLE_SVC_CHR_FLAG_NOTIFY)
    {
        out |= BLE_GATT_CHR_F_NOTIFY;
    }
    if (flags & BLE_SVC_CHR_FLAG_INDICATE)
    {
        out |= BLE_GATT_CHR_F_INDICATE;
    }

    return out;
}

/**
 * @brief  发送通知或指示
 *
 * 通知按当前 MTU 分片，mutex 包住整个分片循环而不是单次发送：一旦两个任务
 * 的分片交替，对端收到的就是一串无法还原的字节流。指示需要协议栈逐条确认，
 * 不分片，超过 MTU - 3 直接报错。
 *
 * @param  conn_handle 目标连接句柄。
 * @param  attr_handle 目标特征的 value handle。
 * @param  data        待发送数据。
 * @param  len         数据长度。
 * @param  indicate    true 发指示，false 发通知。
 * @return 0 成功, -1 参数错误、超长、mbuf 分配失败或发送失败。
 */
static int esp32_ble_send(uint16_t conn_handle, uint16_t attr_handle,
                          const uint8_t *data, uint16_t len, bool indicate)
{
    struct os_mbuf *om;
    uint16_t        chunk_max;
    uint16_t        offset = 0;
    uint16_t        chunk_len;
    bool            locked = false;
    int             rc = 0;

    if (!data || len == 0)
    {
        return -1;
    }

    chunk_max = ble_att_mtu(conn_handle);
    if (chunk_max <= ESP32_BLE_ATT_HEADER_LEN)
    {
        ESP_LOGE(TAG, "invalid mtu: %d", chunk_max);
        return -1;
    }
    chunk_max -= ESP32_BLE_ATT_HEADER_LEN;

    if (indicate && len > chunk_max)
    {
        ESP_LOGE(TAG, "indicate data too long: %d > %d", len, chunk_max);
        return -1;
    }

    if (g_notify_mutex)
    {
        if (xSemaphoreTake(g_notify_mutex, portMAX_DELAY) != pdTRUE)
        {
            return -1;
        }
        locked = true;
    }

    if (indicate)
    {
        om = ble_hs_mbuf_from_flat(data, len);
        if (!om)
        {
            rc = -1;
        }
        else if (ble_gatts_indicate_custom(conn_handle, attr_handle, om) != 0)
        {
            rc = -1;
        }
    }
    else
    {
        while (offset < len)
        {
            chunk_len = (len - offset) > chunk_max ? chunk_max : (len - offset);
            om = ble_hs_mbuf_from_flat(data + offset, chunk_len);
            if (!om)
            {
                rc = -1;
                break;
            }

            if (ble_gatts_notify_custom(conn_handle, attr_handle, om) != 0)
            {
                rc = -1;
                break;
            }

            offset += chunk_len;
        }
    }

    if (locked)
    {
        xSemaphoreGive(g_notify_mutex);
    }

    if (rc != 0)
    {
        ESP_LOGE(TAG, "send failed, indicate=%d len=%d", indicate, len);
    }

    return rc;
}

/**
 * @brief  把 6 字节地址格式化成字符串
 *
 * @param addr_str 输出缓冲，至少 18 字节。
 * @param addr     BLE 地址。
 */
static void esp32_ble_format_addr(char *addr_str, uint8_t addr[])
{
    snprintf(addr_str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

/**
 * @brief  打印连接参数
 *
 * @param desc 连接描述符。
 */
static void esp32_ble_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(TAG, "conn: itvl=%d latency=%d timeout=%d",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout);
}

/**
 * @brief  GATT 属性注册回调
 *
 * @param ctxt 注册上下文。
 * @param arg  未使用。
 */
static void esp32_ble_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt,
                                       void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    (void)arg;

    switch (ctxt->op)
    {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered svc %s handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registered chr %s def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
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
/*                              EOF                                         */
/****************************************************************************/
