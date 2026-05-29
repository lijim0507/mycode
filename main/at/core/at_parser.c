/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "at.h"

#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define AT_INTER_FRAME_MS   200                              /* 帧间超时(ms)，URC 无结束标志时的兜底 */
/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef struct
{
    const char      *prefix;
    at_urc_handler_t handler;
    void            *user_data;
} at_urc_entry_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static bool at_parser_check_format(const char *data);
static void at_parser_dispatch_urc(const char *line);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/* ── 阶段一：帧积累 ── */
static char     g_accum_buf[AT_RESP_BUF_SIZE];              /* 原始数据积累缓冲               */
static uint32_t g_accum_len;                                /* 已积累长度                     */
static uint64_t g_last_tick;                                /* 最后收到数据的时刻             */

/* ── 阶段二：帧分析 ── */
static at_urc_entry_t g_urc_table[AT_URC_MAX_ENTRIES];     /* URC 注册表                    */
static uint8_t        g_urc_count;                          /* 已注册 URC 数量               */

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/* ==================== 阶段一：帧完成性检测 ==================== */

/*
 * 向积累器喂入原始数据。
 * 检查积累的数据中是否已出现帧结束标志 (OK/ERROR)。
 * 返回值: 1=帧格式完整, 0=继续积累
 */
int at_parser_feed(const char *data, uint32_t len)
{
    if (!data || len == 0)
    {
        return 0;
    }

    /* 追加到积累缓冲 */
    if (g_accum_len + len >= AT_RESP_BUF_SIZE)
    {
        len = AT_RESP_BUF_SIZE - g_accum_len - 1;
    }
    if (len > 0)
    {
        memcpy(g_accum_buf + g_accum_len, data, len);
        g_accum_len += len;
        g_accum_buf[g_accum_len] = '\0';
    }

    g_last_tick = pdTICKS_TO_MS(xTaskGetTickCount());

    /* 检查帧格式是否完整 */
    if (at_parser_check_format(g_accum_buf))
    {
        return 1;
    }

    return 0;
}

/*
 * 检查帧间超时：从最后一次收到数据至今是否超过 timeout_ms。
 * 用于 URC 等无结束标志的帧完成判断。
 * 返回值: 1=超时(帧视为完整), 0=仍在等待
 */
int at_parser_check_timeout(uint32_t timeout_ms)
{
    if (g_accum_len == 0)
    {
        return 0;
    }

    if (timeout_ms == 0)
    {
        timeout_ms = AT_INTER_FRAME_MS;
    }

    uint64_t now = pdTICKS_TO_MS(xTaskGetTickCount());
    if (now - g_last_tick > timeout_ms)
    {
        return 1;
    }

    return 0;
}

/*
 * 取出当前积累的完整帧数据，并复位积累器准备接收下一帧。
 */
void at_parser_get_frame(char *buf, uint32_t *out_len)
{
    if (buf && out_len)
    {
        memcpy(buf, g_accum_buf, g_accum_len);
        *out_len = g_accum_len;
    }

    g_accum_len = 0;
    memset(g_accum_buf, 0, sizeof(g_accum_buf));
}

/* ==================== 阶段二：帧内容分析 ==================== */

/*
 * 对完整帧进行逐行分析。
 * - URC 行 → 立即分发到注册的 handler
 * - OK/ERROR 行 → 标记为命令响应结束
 * - 其余行 → 累积为响应 payload
 *
 * 返回值: 命令响应状态 (AT_RESP_OK / AT_RESP_ERROR)
 *         若帧中无 OK/ERROR (纯 URC)，返回 AT_RESP_TIMEOUT 表示"无命令结果"
 */
at_resp_status_t at_parser_analyze(const char *frame, uint32_t frame_len,
                                    char *resp_data, uint32_t *resp_len)
{
    uint32_t rlen = 0;
    char     line_buf[256];
    uint32_t line_len = 0;
    bool     skip_echo = true;
    at_resp_status_t result = AT_RESP_TIMEOUT;

    if (!frame || !resp_data || !resp_len)
    {
        return AT_RESP_TIMEOUT;
    }

    memset(resp_data, 0, AT_RESP_BUF_SIZE);
    *resp_len = 0;

    for (uint32_t i = 0; i < frame_len; i++)
    {
        char ch = frame[i];

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            /* 完成一行 */
            if (line_len > 0)
            {
                line_buf[line_len] = '\0';

                /* 跳过回显行 */
                if (skip_echo)
                {
                    skip_echo = false;
                    line_len = 0;
                    continue;
                }

                /* OK */
                if (strcmp(line_buf, "OK") == 0)
                {
                    result = AT_RESP_OK;
                    line_len = 0;
                    continue;
                }

                /* ERROR */
                if (strcmp(line_buf, "ERROR") == 0
                    || strncmp(line_buf, "+CME ERROR:", 11) == 0)
                {
                    result = AT_RESP_ERROR;
                    line_len = 0;
                    continue;
                }

                /* > 提示符（等待进一步数据输入，如 SMS 文本模式） */
                if (strcmp(line_buf, ">") == 0)
                {
                    /* 不结束帧，继续等 */
                    line_len = 0;
                    continue;
                }

                /* URC 匹配并分发 */
                at_parser_dispatch_urc(line_buf);

                /* 累积为响应 payload */
                if (rlen > 0)
                {
                    resp_data[rlen++] = '\n';
                }
                uint32_t copy = strlen(line_buf);
                if (rlen + copy < AT_RESP_BUF_SIZE)
                {
                    memcpy(resp_data + rlen, line_buf, copy);
                    rlen += copy;
                }
            }
            line_len = 0;
            continue;
        }

        /* 防溢出 */
        if (line_len < sizeof(line_buf) - 1)
        {
            line_buf[line_len++] = ch;
        }
    }

    /* 残留的不完整行也累积 */
    if (line_len > 0)
    {
        line_buf[line_len] = '\0';
        if (rlen > 0)
        {
            resp_data[rlen++] = '\n';
        }
        uint32_t copy = line_len;
        if (rlen + copy < AT_RESP_BUF_SIZE)
        {
            memcpy(resp_data + rlen, line_buf, copy);
            rlen += copy;
        }
    }

    *resp_len = rlen;
    return result;
}

/*
 * 注册 URC 监听前缀（内部，由 at.c 转发）。
 */
int at_parser_urc_register(const char *prefix, at_urc_handler_t handler, void *user_data)
{
    if (!prefix || !handler)
    {
        return -1;
    }

    if (g_urc_count >= AT_URC_MAX_ENTRIES)
    {
        return -2;
    }

    g_urc_table[g_urc_count].prefix    = prefix;
    g_urc_table[g_urc_count].handler   = handler;
    g_urc_table[g_urc_count].user_data = user_data;
    g_urc_count++;

    return 0;
}

/*
 * 注销 URC 监听前缀（内部，由 at.c 转发）。
 */
int at_parser_urc_unregister(const char *prefix)
{
    if (!prefix)
    {
        return -1;
    }

    for (int i = 0; i < g_urc_count; i++)
    {
        if (strcmp(g_urc_table[i].prefix, prefix) == 0)
        {
            g_urc_table[i] = g_urc_table[g_urc_count - 1];
            g_urc_count--;
            return 0;
        }
    }

    return -1;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/*
 * 检查积累数据中是否已出现帧结束标志。
 * 通过搜索 "\nOK\r\n" 或 "\nERROR\r\n" 或 "> " 判断帧完整性。
 */
static bool at_parser_check_format(const char *data)
{
    if (!data || g_accum_len == 0)
    {
        return false;
    }

    /* 搜索结束标志: \nOK\r\n 或 \nERROR\r\n */
    /* 也匹配从行首开始的 OK\r\n */
    if (strstr(data, "OK\r\n") != NULL)
    {
        return true;
    }

    if (strstr(data, "ERROR\r\n") != NULL)
    {
        return true;
    }

    /* 搜索完整帧尾 (以 \n 结尾的 OK 或 ERROR) */
    /* 覆盖无 \r 的情况（某些模块不严格发 \r） */
    const char *ptr = data;
    while ((ptr = strstr(ptr, "\nOK")) != NULL)
    {
        ptr += 3;
        /* \nOK 之后是 \r\n 或 \n 或 结尾 */
        if (ptr[0] == '\r' && ptr[1] == '\n')
        {
            return true;
        }
        if (ptr[0] == '\n' || ptr[0] == '\0')
        {
            return true;
        }
    }

    ptr = data;
    while ((ptr = strstr(ptr, "\nERROR")) != NULL)
    {
        ptr += 6;
        if (ptr[0] == '\r' && ptr[1] == '\n')
        {
            return true;
        }
        if (ptr[0] == '\n' || ptr[0] == '\0')
        {
            return true;
        }
    }

    return false;
}

/*
 * 匹配并分发 URC 行。
 */
static void at_parser_dispatch_urc(const char *line)
{
    for (int i = 0; i < g_urc_count; i++)
    {
        uint32_t prefix_len = strlen(g_urc_table[i].prefix);
        if (strncmp(line, g_urc_table[i].prefix, prefix_len) == 0)
        {
            g_urc_table[i].handler(line, g_urc_table[i].user_data);
            return;
        }
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
