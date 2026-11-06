
#ifndef __BLE_LOG_H_
#define __BLE_LOG_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#include "ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/* 编译期裁剪级别：0=NONE 1=ERROR 2=WARN 3=INFO 4=DEBUG 5=VERBOSE，
   低于该级别的宏直接展开为空，格式串不进二进制 */
#ifndef BLE_LOG_OUTPUT_LEVEL
#define BLE_LOG_OUTPUT_LEVEL        5
#endif

/* 单行最大字节数（含结尾 '\n'），行缓冲分配在调用者栈上 */
#ifndef BLE_LOG_LINE_MAX
#define BLE_LOG_LINE_MAX            256
#endif

/* 日志服务与特征 UUID（本仓库内部约定，换工程时改这两行即可） */
#define BLE_LOG_SVC_UUID \
    BLE_UUID128_BYTES(0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, \
                      0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x40)
#define BLE_LOG_CHR_UUID \
    BLE_UUID128_BYTES(0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, \
                      0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x50)

#if (BLE_LOG_OUTPUT_LEVEL >= 1)
#define BLE_LOG_E(tag, format, ...) ble_log_write(BLE_LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#else
#define BLE_LOG_E(tag, format, ...) ((void)0)
#endif

#if (BLE_LOG_OUTPUT_LEVEL >= 2)
#define BLE_LOG_W(tag, format, ...) ble_log_write(BLE_LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#else
#define BLE_LOG_W(tag, format, ...) ((void)0)
#endif

#if (BLE_LOG_OUTPUT_LEVEL >= 3)
#define BLE_LOG_I(tag, format, ...) ble_log_write(BLE_LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#else
#define BLE_LOG_I(tag, format, ...) ((void)0)
#endif

#if (BLE_LOG_OUTPUT_LEVEL >= 4)
#define BLE_LOG_D(tag, format, ...) ble_log_write(BLE_LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#else
#define BLE_LOG_D(tag, format, ...) ((void)0)
#endif

#if (BLE_LOG_OUTPUT_LEVEL >= 5)
#define BLE_LOG_V(tag, format, ...) ble_log_write(BLE_LOG_LEVEL_VERBOSE, tag, format, ##__VA_ARGS__)
#else
#define BLE_LOG_V(tag, format, ...) ((void)0)
#endif

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/**
 * @brief 日志级别，取值顺序与 ESP-IDF 的 esp_log_level_t 一致。
 *
 * 顺序刻意对齐，ESP32 侧需要转换时可以直接强转，零成本。
 */
typedef enum {
    BLE_LOG_LEVEL_NONE = 0,
    BLE_LOG_LEVEL_ERROR,
    BLE_LOG_LEVEL_WARN,
    BLE_LOG_LEVEL_INFO,
    BLE_LOG_LEVEL_DEBUG,
    BLE_LOG_LEVEL_VERBOSE
} ble_log_level_t;

/**
 * @brief 日志模块配置。
 */
typedef struct {
    uint32_t        (*get_time_ms)(void);   /**< 时间源，可为 NULL；NULL 时行内不输出时间戳 */
    ble_log_level_t   level;                /**< 运行期过滤级别，不超过编译期 BLE_LOG_OUTPUT_LEVEL */
} ble_log_config_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

/**
 * @brief  初始化 BLE 日志模块，内部注册日志服务。
 * @param  config 日志配置，不能为空。
 * @return 0 成功, -1 参数错误, -2 服务注册失败（未在 ble_init() 之前调用或容量不足）
 * @note   必须在 ble_init() 之前调用；日志服务注册后无法注销，ble_log_deinit() 只清本地状态。
 */
int  ble_log_init(const ble_log_config_t *config);

/**
 * @brief  停止写日志。
 *
 * @return 0 始终成功。
 * @note   只清本地状态；服务与特征已注册进协议栈，无法注销。
 */
int  ble_log_deinit(void);

/**
 * @brief  设置运行期过滤级别。
 *
 * @param  level 目标级别，超出范围的值被忽略。
 * @note   只能比编译期 BLE_LOG_OUTPUT_LEVEL 更严格，放宽无效。
 */
void ble_log_set_level(ble_log_level_t level);

/**
 * @brief  格式化一行日志并通过 BLE 通知特征发出。
 *
 * @param  level  日志级别，取 ble_log_level_t。
 * @param  tag    日志标签，NULL 按空串处理。
 * @param  format printf 风格格式串，NULL 时直接返回。
 * @note   行格式固定为 "I (1234567) TAG: 正文\n"，时间源为 NULL 时省略括号段。
 *         未连接或对端未订阅时在格式化之前就返回。一般用 BLE_LOG_E/W/I/D/V 宏调用。
 */
void ble_log_write(ble_log_level_t level, const char *tag, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
