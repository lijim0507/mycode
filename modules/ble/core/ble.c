/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ble.h"
#include "ble_port.h"

#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define TAG "BLE"

#define BLE_SUB_TABLE_SIZE   (BLE_SVC_MAX * BLE_SVC_MAX_CHR)
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/* 单个特征的订阅状态。CCCD 属于连接而非全局，断开时整表作废。 */
typedef struct {
    uint16_t attr_handle;
    bool     notify_enabled;
} ble_sub_entry_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void ble_subscribe_cb(uint16_t conn_handle, uint16_t attr_handle,
                             bool notify_enabled);
static void ble_conn_cb(uint16_t conn_handle, bool connected);

static void ble_sub_update(uint16_t attr_handle, bool notify_enabled);
static bool ble_sub_enabled(uint16_t attr_handle);
static void ble_sub_clear(void);

static int  ble_svc1_chr1_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                    uint8_t op, const uint8_t *data, uint16_t len);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const ble_driver_t *g_driver;
static bool                g_initialized;

/* 连接状态与订阅状态，均由 port 回调写入 */
static uint16_t         g_conn_handle = BLE_CONN_HANDLE_NONE;
static ble_sub_entry_t  g_subs[BLE_SUB_TABLE_SIZE];
static uint8_t          g_sub_num;

/* 内置服务 1 */
static uint16_t g_svc1_chr1_val_handle;
static uint16_t g_svc1_chr2_val_handle;

static uint8_t g_svc1_chr1_rx_buf[64];

static ble_svc_chr_t g_svc1_chrs[] = {
    {
        .uuid        = BLE_UUID128_BYTES(0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                         0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20),
        .flags       = BLE_SVC_CHR_FLAG_WRITE | BLE_SVC_CHR_FLAG_READ |
                       BLE_SVC_CHR_FLAG_NOTIFY,
        .val_handle  = &g_svc1_chr1_val_handle,
        .access_cb   = ble_svc1_chr1_access_cb,
        .subscribe_cb = NULL,
    },
    {
        .uuid        = BLE_UUID128_BYTES(0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                                         0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30),
        .flags       = BLE_SVC_CHR_FLAG_WRITE | BLE_SVC_CHR_FLAG_READ,
        .val_handle  = &g_svc1_chr2_val_handle,
        .access_cb   = ble_svc1_chr1_access_cb,
        .subscribe_cb = NULL,
    },
};

static const ble_svc_def_t g_svc1 = {
    .uuid    = BLE_UUID128_BYTES(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10),
    .chrs    = g_svc1_chrs,
    .chr_num = sizeof(g_svc1_chrs) / sizeof(g_svc1_chrs[0]),
};

/* 已注册服务表：第 0 项固定为内置服务 1，其余由 ble_svc_register() 在 ble_init() 之前追加 */
static const ble_svc_def_t *g_svc_list[BLE_SVC_MAX] = { &g_svc1 };
static uint8_t              g_svc_num = 1;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  启动协议栈并注册 GATT 服务表。
 *
 * driver->init 先把协议栈拉起来，再把服务表连同订阅/连接回调一并交给
 * driver->svc_init。表内容早在 ble_svc_register() 时就已固定，这里只负责传递。
 *
 * @param  driver   平台驱动表，通常取 ble_port_get_driver() 的返回值。
 * @param  port_cfg 原样透传给 driver->init，port 自封装硬件参数后可传 NULL。
 * @return 0 成功, -1 驱动表不完整, -2 driver 初始化失败, -3 服务初始化失败。
 */
int ble_init(const ble_driver_t *driver, void *port_cfg)
{
    ble_svc_table_t table;
    int             rc;

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

    table.svcs  = g_svc_list;
    table.count = g_svc_num;

    rc = driver->svc_init(&table, ble_subscribe_cb, ble_conn_cb);
    if (rc != 0) {
        ESP_LOGE(TAG, "svc init failed, rc=%d", rc);
        if (driver->deinit) {
            driver->deinit();
        }
        return -3;
    }

    g_driver      = driver;
    g_initialized = true;
    return 0;
}

/**
 * @brief  停止协议栈并清空连接/订阅状态。
 *
 * @return 0 始终成功；未初始化时直接返回。
 * @note   已注册的服务不会被移除，重新 ble_init() 时会沿用同一张表。
 */
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
    g_conn_handle = BLE_CONN_HANDLE_NONE;
    ble_sub_clear();
    return 0;
}

/**
 * @brief  协议栈宿主任务，内部进入 nimble_port_run() 后不再返回。
 *
 * @param  param 未使用，为满足 xTaskCreate 的函数签名而保留。
 * @note   由调用方用 xTaskCreate 创建；任务退出时自行 vTaskDelete(NULL)。
 */
void ble_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLE host task started");

    if (g_driver && g_driver->run) {
        g_driver->run();
    }

    vTaskDelete(NULL);
}

/**
 * @brief  往服务表尾部追加一个自定义服务。
 *
 * 按固定顺序做三道检查：描述表完整、host 尚未启动、表还有空位。
 * 先查 g_initialized 再查容量，让"调用时机错了"和"表满了"返回不同错误码。
 *
 * @param  svc 服务描述表，生命周期由调用方保证（静态存储）。
 * @return 0 成功, -1 描述表不完整, -2 服务表已满, -3 host 已启动。
 * @note   NimBLE 的 GATT 表只能在初始化时一次性注册，晚于 ble_init() 无法补救。
 */
int ble_svc_register(const ble_svc_def_t *svc)
{
    if (!svc || !svc->chrs || svc->chr_num == 0 ||
        svc->chr_num > BLE_SVC_MAX_CHR) {
        return -1;
    }

    if (g_initialized) {
        return -3;
    }

    if (g_svc_num >= BLE_SVC_MAX) {
        return -2;
    }

    g_svc_list[g_svc_num] = svc;
    g_svc_num++;
    return 0;
}

/**
 * @brief  判断该特征此刻能否发通知。
 *
 * 三个条件都满足才返回 true：模块已初始化、已连接、对端已订阅该特征。
 * 订阅状态必须查，否则未订阅时调 notify 会白走一遍 mbuf 分配再被协议栈拒绝。
 *
 * @param  attr_handle 特征的 value handle，由 port 在 GATT 注册后回写。
 * @return true 可以发送，false 未初始化、未连接或未订阅。
 * @note   只读状态，不做格式化也不分配内存，可在高频路径上调用。
 */
bool ble_notify_ready(uint16_t attr_handle)
{
    if (!g_initialized || !g_driver || !g_driver->notify) {
        return false;
    }

    if (attr_handle == 0 || g_conn_handle == BLE_CONN_HANDLE_NONE) {
        return false;
    }

    return ble_sub_enabled(attr_handle);
}

/**
 * @brief  向指定特征发一次通知。
 *
 * core 只做就绪判断和转发，分片与链路互斥都在 port 的 notify 里，
 * 任何通过 ble_svc_register() 接入的服务都共用同一套发送语义。
 *
 * @param  attr_handle 特征的 value handle。
 * @param  data        待发送数据，不能为空。
 * @param  len         数据长度，不能为 0。
 * @return 0 成功, -1 参数错误, -2 未就绪, -3 port 发送失败。
 */
int ble_notify_attr(uint16_t attr_handle, const uint8_t *data, uint16_t len)
{
    int rc;

    if (!data || len == 0) {
        return -1;
    }

    if (!ble_notify_ready(attr_handle)) {
        return -2;
    }

    rc = g_driver->notify(g_conn_handle, attr_handle, data, len);
    if (rc != 0) {
        return -3;
    }

    return 0;
}

/**
 * @brief  旧接口：按服务号向内置服务 1 的特征 1 发通知。
 *
 * @param  svc_id 服务号，当前只有 1 有效。
 * @param  data   待发送数据。
 * @param  len    数据长度。
 * @return 0 成功, -2 服务号无效或未就绪, -3 port 发送失败。
 * @note   新增代码请直接用 ble_notify_attr()。
 */
int ble_notify(uint8_t svc_id, const uint8_t *data, uint16_t len)
{
    if (svc_id != 1) {
        return -2;
    }

    return ble_notify_attr(g_svc1_chr1_val_handle, data, len);
}

/**
 * @brief  旧接口：按通道号发送，等价于 ble_notify()。
 *
 * @param  channel 通道号，当前只有 1 有效。
 * @param  data    待发送数据。
 * @param  len     数据长度。
 * @return 0 成功, 其它值见 ble_notify()。
 */
int ble_send(uint8_t channel, const uint8_t *data, uint16_t len)
{
    return ble_notify(channel, data, len);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief  port 上报订阅状态时的入口。
 *
 * 先更新 core 自己的订阅表（供 ble_notify_ready 查询），再按 attr_handle
 * 反查是哪个服务的哪个特征，把事件转给该特征注册的回调。
 *
 * @param  conn_handle    发起订阅的连接句柄。
 * @param  attr_handle    发生订阅变化的特征 value handle。
 * @param  notify_enabled true 表示对端已打开通知。
 */
static void ble_subscribe_cb(uint16_t conn_handle, uint16_t attr_handle,
                             bool notify_enabled)
{
    uint8_t index;

    (void)conn_handle;

    ble_sub_update(attr_handle, notify_enabled);

    for (index = 0; index < g_svc_num; index++) {
        const ble_svc_def_t *svc = g_svc_list[index];
        uint8_t              chr_index;

        for (chr_index = 0; chr_index < svc->chr_num; chr_index++) {
            ble_svc_chr_t *chr = &svc->chrs[chr_index];

            if (!chr->val_handle || *chr->val_handle != attr_handle) {
                continue;
            }

            if (chr->subscribe_cb) {
                chr->subscribe_cb(conn_handle, attr_handle, notify_enabled);
            }
            return;
        }
    }
}

/**
 * @brief  port 上报连接状态时的入口。
 *
 * 断开时清空订阅表：CCCD 状态属于单条连接，旧连接的订阅不能带到新连接上，
 * 否则重连后会在对端尚未订阅时就开始发 notify。
 *
 * @param  conn_handle 连接句柄。
 * @param  connected   true 表示刚建立连接，false 表示已断开。
 */
static void ble_conn_cb(uint16_t conn_handle, bool connected)
{
    if (connected) {
        g_conn_handle = conn_handle;
        return;
    }

    /* 订阅状态属于单条连接，断开后整表失效 */
    g_conn_handle = BLE_CONN_HANDLE_NONE;
    ble_sub_clear();
}

/**
 * @brief  写入或更新一条订阅状态。
 *
 * @param  attr_handle    特征 value handle，为 0 时直接忽略。
 * @param  notify_enabled true 表示已订阅。
 * @note   表满时丢弃新条目：宁可少发一条通知，也不覆盖已有记录的订阅状态。
 */
static void ble_sub_update(uint16_t attr_handle, bool notify_enabled)
{
    uint8_t index;

    if (attr_handle == 0) {
        return;
    }

    for (index = 0; index < g_sub_num; index++) {
        if (g_subs[index].attr_handle == attr_handle) {
            g_subs[index].notify_enabled = notify_enabled;
            return;
        }
    }

    if (g_sub_num >= BLE_SUB_TABLE_SIZE) {
        ESP_LOGW(TAG, "sub table full, attr=%d", attr_handle);
        return;
    }

    g_subs[g_sub_num].attr_handle    = attr_handle;
    g_subs[g_sub_num].notify_enabled = notify_enabled;
    g_sub_num++;
}

/**
 * @brief  查询某个特征是否已被对端订阅。
 *
 * @param  attr_handle 特征 value handle。
 * @return true 已订阅，false 未订阅或表里没有记录。
 */
static bool ble_sub_enabled(uint16_t attr_handle)
{
    uint8_t index;

    for (index = 0; index < g_sub_num; index++) {
        if (g_subs[index].attr_handle == attr_handle) {
            return g_subs[index].notify_enabled;
        }
    }

    return false;
}

/**
 * @brief  清空订阅表，连接断开或模块反初始化时调用。
 */
static void ble_sub_clear(void)
{
    g_sub_num = 0;
}

/**
 * @brief  内置服务 1 的写入回调。
 *
 * 只负责把数据落到本地缓冲，业务解析由应用在自己的服务回调里做，
 * 这样 core 不会反向依赖上层的协议模块。
 *
 * @param  conn_handle 发起访问的连接句柄。
 * @param  attr_handle 被访问的特征 value handle。
 * @param  op          访问类型，取 ble_svc_access_t。
 * @param  data        写入数据，仅 WRITE 有效。
 * @param  len         写入长度，仅 WRITE 有效。
 * @return 0 成功, -1 数据为空或超出缓冲。
 */
static int ble_svc1_chr1_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   uint8_t op, const uint8_t *data, uint16_t len)
{
    (void)conn_handle;
    (void)attr_handle;

    if (op == BLE_SVC_ACCESS_WRITE) {
        if (!data || len == 0 || len > sizeof(g_svc1_chr1_rx_buf)) {
            return -1;
        }

        memcpy(g_svc1_chr1_rx_buf, data, len);
        ESP_LOGI(TAG, "svc1 write, len=%d", len);
    }

    return 0;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
