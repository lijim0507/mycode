
#ifndef __BLE_H_
#define __BLE_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#include "ble_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
/* 未连接时的连接句柄，与 NimBLE 的 BLE_HS_CONN_HANDLE_NONE 取值一致 */
#define BLE_CONN_HANDLE_NONE             0xFFFF

/* 广播包里的 LE role 字段：外设 */
#define BLE_GAP_LE_ROLE_PERIPHERAL       0x00

/* 特征属性抽象位，由 port 翻译成平台原生 flag */
#define BLE_SVC_CHR_FLAG_READ            0x0001
#define BLE_SVC_CHR_FLAG_WRITE           0x0002
#define BLE_SVC_CHR_FLAG_WRITE_NO_RSP    0x0004
#define BLE_SVC_CHR_FLAG_NOTIFY          0x0008
#define BLE_SVC_CHR_FLAG_INDICATE        0x0010

/* 16 位 UUID，按低字节在前的顺序展开。
   刻意不叫 BLE_UUID16_INIT：NimBLE 的 host/ble_uuid.h 已占用该名字，
   port 同时包含两边时会撞成宏重定义。 */
#define BLE_UUID16_BYTES(uuid16)                                        \
    {                                                                   \
        .len = 2,                                                       \
        .bytes = { (uint8_t)((uuid16) & 0xFF),                          \
                   (uint8_t)(((uuid16) >> 8) & 0xFF) }                  \
    }

/* 128 位 UUID 按平台 UUID_INIT 宏的字节序填写（低字节在前，与 UUID 文本串相反） */
#define BLE_UUID128_BYTES(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
    {                                                                     \
        .len = 16,                                                        \
        .bytes = { (a), (b), (c), (d), (e), (f), (g), (h),                \
                   (i), (j), (k), (l), (m), (n), (o), (p) }               \
    }

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/**
 * @brief UUID，支持 16 位和 128 位两种长度。
 *
 * bytes 按低字节在前存放，与平台 UUID_INIT 宏一致，和 UUID 文本串相反。
 * 不直接用平台的 ble_uuid16_t/ble_uuid128_t，是为了让 core 和调用方都不依赖 NimBLE 头。
 */
typedef struct {
    uint8_t len;        /**< 2 表示 16 位 UUID，16 表示 128 位 UUID */
    uint8_t bytes[16];  /**< UUID 原始字节，低字节在前 */
} ble_svc_uuid_t;

/**
 * @brief  特征被读取时的回调，由 port 在 NimBLE 访问回调里转发。
 *
 * @param  conn_handle 发起访问的连接句柄。
 * @param  attr_handle 被访问的特征 value handle。
 * @param  data        输出缓冲区，由 port 提供，回调把要返回的数据写进来。
 * @param  max_len     data 的容量，已按 MTU 和 BLE_CHR_READ_MAX 夹取。
 * @param  out_len     实际写入的字节数，不能超过 max_len。
 * @return 0 成功，其它值由 port 翻译成 ATT 错误码。
 */
typedef int (*ble_chr_read_cb_t)(uint16_t conn_handle, uint16_t attr_handle,
                                 uint8_t *data, uint16_t max_len,
                                 uint16_t *out_len);

/**
 * @brief  特征被写入时的回调，由 port 在 NimBLE 访问回调里转发。
 *
 * @param  conn_handle 发起访问的连接句柄。
 * @param  attr_handle 被访问的特征 value handle。
 * @param  data        写入数据，指向 port 内部的栈缓冲，回调返回即失效。
 * @param  len         写入长度。
 * @return 0 成功，其它值由 port 翻译成 ATT 错误码。
 * @note   回调运行在协议栈宿主任务上下文；耗时操作和回发通知应转交应用任务，
 *         不要在这里再次进入协议栈。
 */
typedef int (*ble_chr_write_cb_t)(uint16_t conn_handle, uint16_t attr_handle,
                                  const uint8_t *data, uint16_t len);

/**
 * @brief  特征 CCCD 订阅状态变化回调，可选（NULL 表示不关心）。
 *
 * @param  conn_handle      发起订阅的连接句柄。
 * @param  attr_handle      发生变化的特征 value handle。
 * @param  notify_enabled   true 表示对端已打开该特征的通知。
 * @param  indicate_enabled true 表示对端已打开该特征的指示。
 */
typedef void (*ble_chr_subscribe_cb_t)(uint16_t conn_handle,
                                       uint16_t attr_handle,
                                       bool notify_enabled,
                                       bool indicate_enabled);

/**
 * @brief 一个特征的定义。
 *
 * 整个结构由调用方以静态存储期持有，port 只在注册时读取，运行期不复制。
 * read_cb / write_cb 按特征支持的能力填写，不支持的留 NULL。
 */
typedef struct {
    ble_svc_uuid_t         uuid;
    uint16_t               flags;        /**< BLE_SVC_CHR_FLAG_xxx 的按位或 */
    uint16_t              *val_handle;   /**< 指向调用方自己的变量，由 port 在 GATT 注册后回写；NULL 表示不需要 */
    ble_chr_read_cb_t      read_cb;      /**< 只写特征可以留 NULL */
    ble_chr_write_cb_t     write_cb;     /**< 只读或纯通知特征可以留 NULL */
    ble_chr_subscribe_cb_t subscribe_cb; /**< 可选 */
} ble_svc_chr_t;

/**
 * @brief 一个服务的定义。
 */
typedef struct {
    ble_svc_uuid_t uuid;
    ble_svc_chr_t *chrs;     /**< 指向静态特征数组 */
    uint8_t        chr_num;  /**< 特征数组元素个数 */
} ble_svc_def_t;

/**
 * @brief core 传给 port 的完整服务表。
 * @note  由 core 内部维护，调用方不直接构造。
 */
typedef struct {
    const ble_svc_def_t **svcs;   /**< 服务定义指针数组 */
    uint8_t               count;  /**< 数组有效元素个数 */
} ble_svc_table_t;

/**
 * @brief  port 上报给 core 的订阅状态，core 据此判断能不能发通知/指示。
 *
 * @param  conn_handle      发起订阅的连接句柄。
 * @param  attr_handle      发生变化的特征 value handle。
 * @param  notify_enabled   true 表示对端已打开通知。
 * @param  indicate_enabled true 表示对端已打开指示。
 */
typedef void (*ble_subscribe_cb_t)(uint16_t conn_handle, uint16_t attr_handle,
                                   bool notify_enabled, bool indicate_enabled);

/**
 * @brief  port 上报给 core 的连接状态，断开时 core 会清空订阅表。
 *
 * @param  conn_handle 连接句柄。
 * @param  connected   true 表示刚建立连接，false 表示已断开。
 */
typedef void (*ble_conn_cb_t)(uint16_t conn_handle, bool connected);

/**
 * @brief 平台驱动表，由 port 实现并通过 ble_port_get_driver() 返回。
 */
typedef struct ble_driver {
    int  (*init)(void);
    int  (*deinit)(void);
    void (*run)(void);
    int  (*adv_start)(void);
    int  (*adv_stop)(void);
    int  (*svc_init)(const ble_svc_table_t *table,
                     ble_subscribe_cb_t subscribe_cb,
                     ble_conn_cb_t conn_cb);
    int  (*notify)(uint16_t conn_handle, uint16_t attr_handle,
                   const uint8_t *data, uint16_t len);
    int  (*indicate)(uint16_t conn_handle, uint16_t attr_handle,
                     const uint8_t *data, uint16_t len);
} ble_driver_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

/**
 * @brief  初始化 BLE 模块：启动协议栈、注册 GATT 服务表、挂上回调。
 *
 * core 内部调用 ble_port_get_driver() 获取平台驱动，调用方不需要传驱动表。
 * 所有服务必须先用 ble_svc_register() 注册，NimBLE 的 GATT 表在初始化时
 * 一次性注册，晚于本函数就无法追加。
 *
 * @return 0 成功, -1 驱动表不完整, -2 driver 初始化失败, -3 服务初始化失败
 * @note   重复调用会先 ble_deinit() 再重新初始化；旧的宿主任务会随之退出，
 *         调用方需要重新创建 ble_task。
 */
int  ble_init(void);

/**
 * @brief  停止协议栈并清空连接/订阅状态。
 *
 * @return 0 成功，port 停止失败时返回 -1；未初始化时直接返回 0。
 * @note   已注册的服务不会被移除，重新 ble_init() 时会沿用同一张表。
 *         不要在 BLE 回调或宿主任务内部调用，nimble_port_stop() 会等待宿主任务退出。
 */
int  ble_deinit(void);

/**
 * @brief  协议栈宿主任务，内部进入 nimble_port_run() 后不再返回。
 *
 * @param  param 未使用，为满足 xTaskCreate 的函数签名而保留。
 * @note   由调用方用 xTaskCreate 创建，栈大小参考 BLE_HOST_TASK_STACK，
 *         优先级参考 ESP-IDF 官方 NimBLE 集成（configMAX_PRIORITIES - 4）。
 *         任务退出时自行 vTaskDelete(NULL)。
 */
void ble_task(void *param);

/**
 * @brief  注册一个自定义服务，必须在 ble_init() 之前调用。
 * @param  svc 服务描述表，生命周期由调用方保证（静态存储）。
 * @return 0 成功, -1 参数错误, -2 容量不足, -3 host 已启动
 * @note   NimBLE 的 GATT 表只能在初始化时一次性注册，事后无法追加，
 *         因此这一步晚于 ble_init() 就只能失败，不能补救。
 */
int  ble_svc_register(const ble_svc_def_t *svc);

/**
 * @brief  手动开始广播。
 *
 * @return 0 成功, -1 未初始化或驱动不支持, -2 port 启动失败
 * @note   默认由 port 在协议栈 sync 后自动广播；手动 ble_adv_stop() 之后
 *         自动重启会被抑制，直到再次调用本函数。
 */
int  ble_adv_start(void);

/**
 * @brief  手动停止广播，并抑制断开/广播结束后的自动重启。
 *
 * @return 0 成功, -1 未初始化或驱动不支持, -2 port 停止失败
 */
int  ble_adv_stop(void);

/**
 * @brief  查询某个特征当前能否发通知。
 * @param  attr_handle 特征的 value handle，由 port 注册后回写。
 * @return true 表示已初始化、已连接且对端已订阅该特征的通知。
 */
bool ble_notify_ready(uint16_t attr_handle);

/**
 * @brief  查询某个特征当前能否发指示。
 * @param  attr_handle 特征的 value handle，由 port 注册后回写。
 * @return true 表示已初始化、已连接且对端已订阅该特征的指示。
 */
bool ble_indicate_ready(uint16_t attr_handle);

/**
 * @brief  向指定特征发一次通知，内部按 MTU 分片。
 * @param  attr_handle 特征的 value handle。
 * @param  data        待发送数据。
 * @param  len         数据长度。
 * @return 0 成功, -1 参数错误, -2 未就绪, -3 port 发送失败
 * @note   分片与发送的原子性由 port 保证，同一时刻只有一整条消息占用链路。
 */
int  ble_notify_attr(uint16_t attr_handle, const uint8_t *data, uint16_t len);

/**
 * @brief  向指定特征发一次指示。
 * @param  attr_handle 特征的 value handle。
 * @param  data        待发送数据。
 * @param  len         数据长度，不能超过 MTU - 3。
 * @return 0 成功, -1 参数错误, -2 未就绪, -3 port 发送失败（含超过 MTU - 3）
 * @note   指示需要协议栈逐条确认，不做分片。
 */
int  ble_indicate_attr(uint16_t attr_handle, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
