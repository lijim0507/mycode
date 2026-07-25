
#ifndef __LOGGER_H_
#define __LOGGER_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/* ANSI 颜色代码 */
#define LOG_COLOR_BLACK   "30"
#define LOG_COLOR_RED     "31"
#define LOG_COLOR_GREEN   "32"
#define LOG_COLOR_YELLOW  "33"
#define LOG_COLOR_BLUE    "34"
#define LOG_COLOR_MAGENTA "35"
#define LOG_COLOR_CYAN    "36"
#define LOG_COLOR_WHITE   "37"

#define LOG_COLOR(COLOR)  "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR)   "\033[1;" COLOR "m"
#define LOG_RESET_COLOR   "\033[0m"

#define LOG_E(tag, format, ...) logger_write(LOG_LEVEL_ERROR,   tag, format, ##__VA_ARGS__)
#define LOG_W(tag, format, ...) logger_write(LOG_LEVEL_WARN,    tag, format, ##__VA_ARGS__)
#define LOG_I(tag, format, ...) logger_write(LOG_LEVEL_INFO,    tag, format, ##__VA_ARGS__)
#define LOG_D(tag, format, ...) logger_write(LOG_LEVEL_DEBUG,   tag, format, ##__VA_ARGS__)
#define LOG_V(tag, format, ...) logger_write(LOG_LEVEL_VERBOSE, tag, format, ##__VA_ARGS__)

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
} log_level_t;

typedef struct logger_driver {
    void     (*uart_send)(const char *data, uint32_t length);
    uint32_t (*get_time_ms)(void);
} logger_driver_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

int  logger_init(const logger_driver_t *driver, log_level_t level,
                 uint8_t enable_color, uint8_t enable_timestamp);
void logger_set_level(log_level_t level);
void logger_set_color_enable(uint8_t enable);
void logger_set_timestamp_enable(uint8_t enable);
void logger_write(log_level_t level, const char *tag, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
