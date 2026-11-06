
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

/* 未连接时的连接句柄，与 NimBLE 的 BLE_HS_CONN_HANDLE_NONE 取值一致 */
#define BLE_CONN_HANDLE_NONE             0xFFFF

/* 最多可注册的服务数 */
#ifndef BLE_SVC_MAX
#define BLE_SVC_MAX                      4
#endif

/* 单个服务下最多的特征数 */
#ifndef BLE_SVC_MAX_CHR
#define BLE_SVC_MAX_CHR                  4
#endif

/* 特征属性抽象位，由 port 翻译成平台原生 flag */
#define BLE_SVC_CHR_FLAG_READ            0x0001
#define BLE_SVC_CHR_FLAG_WRITE           0x0002
#define BLE_SVC_CHR_FLAG_WRITE_NO_RSP    0x0004
#define BLE_SVC_CHR_FLAG_NOTIFY          0x0008
#define BLE_SVC_CHR_FLAG_INDICATE        0x0010

/* 128 位 UUID 按平台 UUID_INIT 宏的字节序填写（低字节在前，与 UUID 文本串相反） */
#define BLE_UUID128_BYTES(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
    {{ (a), (b), (c), (d), (e), (f), (g), (h),                            \
       (i), (j), (k), (l), (m), (n), (o), (p) }}

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/**
 * @brief 特征访问类型，由 ble_chr_access_cb_t 的 op 参数传入。
 */
typedef enum {
    BLE_SVC_ACCESS_READ = 0,
    BLE_SVC_ACCESS_WRITE,
} ble_svc_access_t;

/**
 * @brief 128 位 UUID。
 *
 * 字节序与平台 UUID_INIT 宏一致（低字节在前，与 UUID 文本串相反）。
 * 不直接用平台的 ble_uuid128_t，是为了让 core 和调用方都不依赖 NimBLE 头。
 */
typedef struct {
    uint8_t bytes[16];
} ble_svc_uuid_t;

/**
 * @brief  特征被读写时的回调，由 port 在 NimBLE 访问回调里转发。
 *
 * @param  conn_handle 发起访问的连接句柄。
 * @param  attr_handle 被访问的特征 value handle。
 * @param  op          访问类型，取 ble_svc_access_t。
 * @param  data        写入数据，仅 WRITE 有效，指向 port 内部的栈缓冲，回调返回即失效。
 * @param  len         写入长度，仅 WRITE 有效。
 * @return 0 成功，其它值由 port 翻译成 ATT 错误码。
 */
typedef int (*ble_chr_access_cb_t)(uint16_t conn_handle, uint16_t attr_handle,
                                   uint8_t op, const uint8_t *data, uint16_t len);

/**
 * @brief  特征 CCCD 订阅状态变化回调，可选（NULL 表示不关心）。
 *
 * @param  conn_handle    发起订阅的连接句柄。
 * @param  attr_handle    发生变化的特征 value handle。
 * @param  notify_enabled true 表示对端已打开该特征的通知。
 */
typedef void (*ble_chr_subscribe_cb_t)(uint16_t conn_handle,
                                       uint16_t attr_handle,
                                       bool notify_enabled);

/**
 * @brief 一个特征的定义。
 *
 * 整个结构由调用方以静态存储期持有，port 只在注册时读取，运行期不复制。
 */
typedef struct {
    ble_svc_uuid_t         uuid;
    uint16_t               flags;        /**< BLE_SVC_CHR_FLAG_xxx 的按位或 */
    uint16_t              *val_handle;   /**< 指向调用方自己的变量，由 port 在 GATT 注册后回写；NULL 表示不需要 */
    ble_chr_access_cb_t    access_cb;    /**< 纯通知特征可以留 NULL */
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
 * @brief  port 上报给 core 的订阅状态，core 据此判断能不能发通知。
 *
 * @param  conn_handle    发起订阅的连接句柄。
 * @param  attr_handle    发生变化的特征 value handle。
 * @param  notify_enabled true 表示对端已打开通知。
 */
typedef void (*ble_subscribe_cb_t)(uint16_t conn_handle, uint16_t attr_handle,
                                   bool notify_enabled);

/**
 * @brief  port 上报给 core 的连接状态，断开时 core 会清空订阅表。
 *
 * @param  conn_handle 连接句柄。
 * @param  connected   true 表示刚建立连接，false 表示已断开。
 */
typedef void (*ble_conn_cb_t)(uint16_t conn_handle, bool connected);

/**
 * @brief 平台驱动表，由 port 实现并通过 xxx_port_get_driver() 返回。
 */
typedef struct ble_driver {
    int  (*init)(void *config);
    int  (*deinit)(void);
    void (*run)(void);
    int  (*adv_start)(void *adv_cfg);
    int  (*svc_init)(const ble_svc_table_t *table,
                     ble_subscribe_cb_t subscribe_cb,
                     ble_conn_cb_t conn_cb);
    int  (*notify)(uint16_t conn_handle, uint16_t attr_handle,
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
 * @param  driver   平台驱动表，通常传 ble_port_get_driver() 的返回值。
 * @param  port_cfg 透传给 driver->init，port 自封装硬件参数后可传 NULL。
 * @return 0 成功, -1 参数错误, -2 driver 初始化失败, -3 服务初始化失败
 * @note   内置服务 1 固定占用服务表第 0 项，ble_svc_register() 追加的服务排在其后。
 */
int  ble_init(const ble_driver_t *driver, void *port_cfg);

/**
 * @brief  停止协议栈并清空连接/订阅状态。
 *
 * @return 0 始终成功；未初始化时直接返回。
 * @note   已注册的服务不会被移除，重新 ble_init() 时会沿用同一张表。
 */
int  ble_deinit(void);

/**
 * @brief  协议栈宿主任务，内部进入 nimble_port_run() 后不再返回。
 *
 * @param  param 未使用，为满足 xTaskCreate 的函数签名而保留。
 * @note   由调用方用 xTaskCreate 创建；任务退出时自行 vTaskDelete(NULL)。
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
 * @brief  查询某个特征当前能否发通知。
 * @param  attr_handle 特征的 value handle，由 port 注册后回写。
 * @return true 表示已初始化、已连接且对端已订阅该特征。
 * @note   只读状态、不做格式化也不分配内存，供上层在生成数据前做快速门控。
 */
bool ble_notify_ready(uint16_t attr_handle);

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
 * @brief  旧接口：按服务号向内置服务 1 的特征 1 发通知。
 *
 * @param  svc_id 服务号，当前只有 1 有效。
 * @param  data   待发送数据。
 * @param  len    数据长度。
 * @return 0 成功, -2 服务号无效或未就绪, -3 port 发送失败。
 * @note   新增代码请直接用 ble_notify_attr()。
 */
int  ble_notify(uint8_t svc_id, const uint8_t *data, uint16_t len);

/**
 * @brief  旧接口：按通道号发送，等价于 ble_notify()。
 *
 * @param  channel 通道号，当前只有 1 有效。
 * @param  data    待发送数据。
 * @param  len     数据长度。
 * @return 0 成功, 其它值见 ble_notify()。
 */
int  ble_send(uint8_t channel, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
