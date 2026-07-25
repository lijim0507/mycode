/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "foc_port.h"

#include <string.h>

#include "driver/ledc.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define FOC_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define FOC_LEDC_DUTY_RES   LEDC_TIMER_13_BIT
#define FOC_LEDC_FREQ_HZ    (20000)
#define FOC_LEDC_CH_NUM     3
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct {
    uint8_t gpio_a;
    uint8_t gpio_b;
    uint8_t gpio_c;
} foc_esp32_cfg_t;

typedef struct {
    bool initialized;
} foc_esp32_ctx_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int  esp32_foc_init(void *config);
static void esp32_foc_write_duty(uint8_t channel, float duty_cycle);
static int  esp32_foc_deinit(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static foc_esp32_ctx_t g_foc_ctx;

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int esp32_foc_init(void *config)
{
    uint8_t gpio_a = 25;
    uint8_t gpio_b = 26;
    uint8_t gpio_c = 27;

    if (config != NULL) {
        foc_esp32_cfg_t *cfg = (foc_esp32_cfg_t *)config;
        gpio_a = cfg->gpio_a;
        gpio_b = cfg->gpio_b;
        gpio_c = cfg->gpio_c;
    }

    ledc_timer_config_t timer_cfg = {
        .speed_mode      = FOC_LEDC_MODE,
        .duty_resolution = FOC_LEDC_DUTY_RES,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = FOC_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };

    if (ledc_timer_config(&timer_cfg) != ESP_OK) {
        return -1;
    }

    ledc_channel_config_t ch_cfg[FOC_LEDC_CH_NUM] = {
        {.gpio_num = gpio_a, .speed_mode = FOC_LEDC_MODE, .channel = LEDC_CHANNEL_0,
         .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0},
        {.gpio_num = gpio_b, .speed_mode = FOC_LEDC_MODE, .channel = LEDC_CHANNEL_1,
         .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0},
        {.gpio_num = gpio_c, .speed_mode = FOC_LEDC_MODE, .channel = LEDC_CHANNEL_2,
         .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0},
    };

    for (int i = 0; i < FOC_LEDC_CH_NUM; i++) {
        if (ledc_channel_config(&ch_cfg[i]) != ESP_OK) {
            ledc_timer_rst(FOC_LEDC_MODE, LEDC_TIMER_0);
            return -2;
        }
    }

    g_foc_ctx.initialized = true;
    return 0;
}

static void esp32_foc_write_duty(uint8_t channel, float duty_cycle)
{
    if (!g_foc_ctx.initialized || channel >= FOC_LEDC_CH_NUM) {
        return;
    }

    uint32_t max_duty = (1U << FOC_LEDC_DUTY_RES) - 1;
    uint32_t duty_val = (uint32_t)(duty_cycle * max_duty);

    ledc_set_duty(FOC_LEDC_MODE, (ledc_channel_t)channel, duty_val);
    ledc_update_duty(FOC_LEDC_MODE, (ledc_channel_t)channel);
}

static int esp32_foc_deinit(void)
{
    if (!g_foc_ctx.initialized) {
        return 0;
    }

    ledc_stop(FOC_LEDC_MODE, LEDC_CHANNEL_0, 0);
    ledc_stop(FOC_LEDC_MODE, LEDC_CHANNEL_1, 0);
    ledc_stop(FOC_LEDC_MODE, LEDC_CHANNEL_2, 0);

    g_foc_ctx.initialized = false;
    return 0;
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const foc_driver_t *foc_port_get_driver(void)
{
    static const foc_driver_t driver = {
        .init       = esp32_foc_init,
        .write_duty = esp32_foc_write_duty,
        .deinit     = esp32_foc_deinit,
    };
    return &driver;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
