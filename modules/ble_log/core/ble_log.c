/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "ble_log.h"

#include <stdio.h>
#include <stdarg.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static bool             g_initialized;
static ble_log_level_t  g_level = BLE_LOG_LEVEL_INFO;
static uint32_t       (*g_get_time_ms)(void);   /* NULL 表示行内不输出时间戳 */

/* 日志特征句柄，由 ble 在 GATT 注册后回写；为 0 说明还没注册完成 */
static uint16_t g_log_val_handle;

/* 只有一个通知特征，不需要 access_cb：NOTIFY-only 没有读写路径 */
static ble_svc_chr_t g_ble_log_chrs[] = {
    {
        .uuid         = BLE_LOG_CHR_UUID,
        .flags        = BLE_SVC_CHR_FLAG_NOTIFY,
        .val_handle   = &g_log_val_handle,
        .access_cb    = NULL,
        .subscribe_cb = NULL,
    },
};

static const ble_svc_def_t g_ble_log_svc = {
    .uuid    = BLE_LOG_SVC_UUID,
    .chrs    = g_ble_log_chrs,
    .chr_num = sizeof(g_ble_log_chrs) / sizeof(g_ble_log_chrs[0]),
};

static const char g_level_chars[] = {' ', 'E', 'W', 'I', 'D', 'V'};
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief  初始化 BLE 日志模块，并把日志服务挂到 ble 的服务表上。
 *
 * 必须先于 ble_init() 调用：NimBLE 的 GATT 表在初始化时一次性注册，
 * 事后无法追加，晚于 ble_init() 会被 ble_svc_register() 挡在 -3。
 *
 * @param  config 日志配置，不能为空。
 * @return 0 成功, -1 参数错误, -2 服务注册失败（时机不对或服务表已满）。
 */
int ble_log_init(const ble_log_config_t *config)
{
    int rc;

    if (!config) {
        return -1;
    }

    if (config->level > BLE_LOG_LEVEL_VERBOSE) {
        return -1;
    }

    if (g_initialized) {
        return 0;
    }

    rc = ble_svc_register(&g_ble_log_svc);
    if (rc != 0) {
        return -2;
    }

    g_get_time_ms = config->get_time_ms;
    g_level       = config->level;
    g_initialized = true;
    return 0;
}

/**
 * @brief  停止写日志。
 *
 * @return 0 始终成功。
 * @note   只清本地状态。服务与特征已经注册进协议栈，无法注销，
 *         再次 ble_log_init() 时不会重复注册。
 */
int ble_log_deinit(void)
{
    g_initialized = false;
    return 0;
}

/**
 * @brief  设置运行期过滤级别。
 *
 * @param  level 目标级别，超出范围的值被忽略。
 * @note   只能比编译期 BLE_LOG_OUTPUT_LEVEL 更严格，放宽无效——
 *         被裁剪掉的宏根本不会发出调用。
 */
void ble_log_set_level(ble_log_level_t level)
{
    if (level > BLE_LOG_LEVEL_VERBOSE) {
        return;
    }

    g_level = level;
}

/**
 * @brief  格式化一行日志并通过 BLE 通知特征发出。
 *
 * 顺序是刻意的：等级过滤 → 就绪查询 → 格式化 → 发送。把就绪查询提到
 * vsnprintf 之前，断连期间每次调用只剩两次整数比较，不会为一条注定
 * 被丢弃的日志付出格式化的栈和 CPU 开销。
 *
 * @param  level  日志级别，取 ble_log_level_t。
 * @param  tag    日志标签，NULL 按空串处理。
 * @param  format printf 风格格式串，NULL 时直接返回。
 * @note   行格式固定为 "I (1234567) TAG: 正文\n"，时间源为 NULL 时省略括号段。
 *         正文超长时截断，末字节始终留给 '\n'，保证对端总能按行切分。
 */
void ble_log_write(ble_log_level_t level, const char *tag, const char *format, ...)
{
    char    line[BLE_LOG_LINE_MAX];
    size_t  line_size = sizeof(line);
    int     offset;
    int     written;
    va_list args;

    if (!g_initialized || !format) {
        return;
    }

    if (level == BLE_LOG_LEVEL_NONE || level > g_level) {
        return;
    }

    /* 未连接或对端未订阅时直接丢弃，不付出格式化开销 */
    if (!ble_notify_ready(g_log_val_handle)) {
        return;
    }

    if (!tag) {
        tag = "";
    }

    if (g_get_time_ms) {
        offset = snprintf(line, line_size, "%c (%lu) %s: ",
                          g_level_chars[level],
                          (unsigned long)g_get_time_ms(), tag);
    } else {
        offset = snprintf(line, line_size, "%c %s: ", g_level_chars[level], tag);
    }

    if (offset < 0) {
        return;
    }

    if ((size_t)offset >= line_size - 1) {
        /* 头部已经占满，正文无法追加 */
        offset            = (int)line_size - 1;
        line[offset - 1]  = '\n';
        ble_notify_attr(g_log_val_handle, (const uint8_t *)line, (uint16_t)offset);
        return;
    }

    va_start(args, format);
    written = vsnprintf(line + offset, line_size - offset, format, args);
    va_end(args);

    if (written < 0) {
        return;
    }

    if ((size_t)written < line_size - offset) {
        offset += written;
    } else {
        /* 正文被截断，末字节留给 '\n' */
        offset = (int)line_size - 1;
    }

    line[offset] = '\n';
    offset++;

    ble_notify_attr(g_log_val_handle, (const uint8_t *)line, (uint16_t)offset);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
