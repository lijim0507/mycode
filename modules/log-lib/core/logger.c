/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "logger.h"
#include "logger_port.h"

#include <string.h>
#include <stdio.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void ms_to_time_string(uint32_t ms, char *buffer, uint16_t buffer_size);
static void log_output(const char *str);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static const logger_driver_t *g_driver;
static bool                   g_initialized;

static log_level_t g_level          = LOG_LEVEL_INFO;
static uint8_t     g_enable_color       = 1;
static uint8_t     g_enable_timestamp   = 1;

static const char *level_strings[] = {
    [LOG_LEVEL_NONE]    = "",
    [LOG_LEVEL_ERROR]   = "E",
    [LOG_LEVEL_WARN]    = "W",
    [LOG_LEVEL_INFO]    = "I",
    [LOG_LEVEL_DEBUG]   = "D",
    [LOG_LEVEL_VERBOSE] = "V"
};

static const char *level_colors[] = {
    [LOG_LEVEL_NONE]    = LOG_COLOR_WHITE,
    [LOG_LEVEL_ERROR]   = LOG_COLOR(LOG_COLOR_RED),
    [LOG_LEVEL_WARN]    = LOG_COLOR(LOG_COLOR_YELLOW),
    [LOG_LEVEL_INFO]    = LOG_COLOR(LOG_COLOR_GREEN),
    [LOG_LEVEL_DEBUG]   = LOG_COLOR(LOG_COLOR_BLUE),
    [LOG_LEVEL_VERBOSE] = LOG_COLOR(LOG_COLOR_MAGENTA)
};
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int logger_init(const logger_driver_t *driver, log_level_t level,
                 uint8_t enable_color, uint8_t enable_timestamp)
{
    if (!driver || !driver->uart_send || !driver->get_time_ms) {
        return -1;
    }

    g_driver              = driver;
    g_level               = level;
    g_enable_color        = enable_color;
    g_enable_timestamp    = enable_timestamp;
    g_initialized         = true;
    return 0;
}

void logger_set_level(log_level_t level)
{
    g_level = level;
}

void logger_set_color_enable(uint8_t enable)
{
    g_enable_color = enable;
}

void logger_set_timestamp_enable(uint8_t enable)
{
    g_enable_timestamp = enable;
}

void logger_write(log_level_t level, const char *tag, const char *format, ...)
{
    if (!g_initialized || !g_driver || level > g_level) {
        return;
    }

    char  message[192];
    char  timestamp[16] = "";
    va_list args;

    if (g_enable_timestamp) {
        uint32_t current_ms = g_driver->get_time_ms();
        ms_to_time_string(current_ms, timestamp, sizeof(timestamp));
    }

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    char log_line[256];

    if (g_enable_color) {
        snprintf(log_line, sizeof(log_line),
                 "%s %s(%s) %s: %s" LOG_RESET_COLOR "\r\n",
                 timestamp, level_colors[level], level_strings[level],
                 tag, message);
    } else {
        snprintf(log_line, sizeof(log_line),
                 "%s (%s) %s: %s\r\n",
                 timestamp, level_strings[level], tag, message);
    }

    log_output(log_line);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static void ms_to_time_string(uint32_t ms, char *buffer, uint16_t buffer_size)
{
    uint32_t hours        = ms / 3600000;
    uint32_t minutes      = (ms % 3600000) / 60000;
    uint32_t seconds      = (ms % 60000) / 1000;
    uint32_t milliseconds = ms % 1000;

    snprintf(buffer, buffer_size, "%02lu:%02lu:%02lu.%03lu",
             hours, minutes, seconds, milliseconds);
}

static void log_output(const char *str)
{
    if (g_driver->uart_send && str) {
        g_driver->uart_send(str, (uint32_t)strlen(str));
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
