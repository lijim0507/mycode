/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "at.h"
#include "at_port.h"

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


/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static at_urc_entry_t g_urc_table[AT_URC_MAX_ENTRIES];     /* URC 注册表                    */
static uint8_t        g_urc_count;                          /* 已注册 URC 数量               */

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/


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
 * 匹配并分发 URC 行。
 * 返回值: 1=已匹配并分发, 0=未匹配
 */
int at_urc_check_and_dispatch(const char *line)
{
    for (int i = 0; i < g_urc_count; i++)
    {
        uint32_t prefix_len = strlen(g_urc_table[i].prefix);
        if (strncmp(line, g_urc_table[i].prefix, prefix_len) == 0)
        {
            g_urc_table[i].handler(line, g_urc_table[i].user_data);
            return 1;
        }
    }
    return 0;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
