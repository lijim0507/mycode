#ifndef __BLE_CONFIG_H_
#define __BLE_CONFIG_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/* 以下为编译期配置，可在编译选项中用 -D 覆盖 */

/* 设备名，广播包里完整带上；超过 31 字节会导致广播字段设置失败 */
#ifndef BLE_DEVICE_NAME
#define BLE_DEVICE_NAME             "MY_BLE_DEVICE"
#endif

/* 协议栈 sync 后是否自动开始广播，断开或广播结束后是否自动重启 */
#ifndef BLE_ADV_AUTO_START
#define BLE_ADV_AUTO_START          1
#endif

/* 广播间隔（ms），最小/最大用于 adv_params */
#ifndef BLE_ADV_ITVL_MIN_MS
#define BLE_ADV_ITVL_MIN_MS         500
#endif

#ifndef BLE_ADV_ITVL_MAX_MS
#define BLE_ADV_ITVL_MAX_MS         510
#endif

/* 广播包里的 appearance 字段 */
#ifndef BLE_ADV_APPEARANCE
#define BLE_ADV_APPEARANCE          0x0200
#endif

/* 连接建立后是否主动请求更新连接参数 */
#ifndef BLE_CONN_UPDATE_ENABLE
#define BLE_CONN_UPDATE_ENABLE      1
#endif

/* 请求的 slave latency，单位：连接事件个数 */
#ifndef BLE_CONN_LATENCY
#define BLE_CONN_LATENCY            3
#endif

/* 协议栈宿主任务栈大小，应用用 xTaskCreate 创建 ble_task 时使用 */
#ifndef BLE_HOST_TASK_STACK
#define BLE_HOST_TASK_STACK         4096
#endif

/* 最多可注册的服务数 */
#ifndef BLE_SVC_MAX
#define BLE_SVC_MAX                 4
#endif

/* 单个服务下最多的特征数 */
#ifndef BLE_SVC_MAX_CHR
#define BLE_SVC_MAX_CHR             4
#endif

/* 单次写入可接收的最大字节数，缓冲分配在协议栈宿主任务栈上 */
#ifndef BLE_CHR_WRITE_MAX
#define BLE_CHR_WRITE_MAX           64
#endif

/* 单次读取可返回的最大字节数，缓冲分配在协议栈宿主任务栈上 */
#ifndef BLE_CHR_READ_MAX
#define BLE_CHR_READ_MAX            64
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
