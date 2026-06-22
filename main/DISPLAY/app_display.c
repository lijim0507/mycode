/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "main.h"
#include "app_display.h"
#include "app_flash.h"
#include "ws2816.h"
#include <string.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#define DISPLAY_RAINBOW_FRAME_MS    30U
#define DISPLAY_RAINBOW_HUE_MS      10U
#define DISPLAY_RAINBOW_GAIN         1U

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/*
 * DISPLAY 点阵屏物理布局 (O=LED存在, X=挖空无LED)
 *
 * Row  0: X X X O O O O O O O O O O X X X     左3 右3
 * Row  1: X X O O O O O O O O O O O O X X     左2 右2
 * Row  2: X O O O O O O O O O O O O O O X     左1 右1
 * Row  3~12: O O O O O O O O O O O O O O O O  无挖空
 * Row 13: X O O O O O O O O O O O O O O X     左1 右1
 * Row 14: X X O O O O O O O O O O O X X     左2 右2
 * Row 15: X X X O O O O O O O O O O X X X     左3 右3
 *
 * 实际LED总数: 232 = 10+12+14+10*16+14+12+10
 */
static const uint8_t s_led_mask[DISPLAY_HEIGHT][DISPLAY_WIDTH] = {
    {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
};



/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static void decode_frame_3bit(const uint8_t *packed, uint8_t *grb_buf);
static void decode_frame_24bit(const uint8_t *rgb_buf, uint8_t *grb_buf);
static void mirror_frame(uint8_t *grb_buf);
static uint16_t display_linear_to_physical(const uint8_t *logical_buffer,
                                           uint8_t *physical_buffer);
static void display_acquire_frame(void);
static void display_send_frame(void);
static void display_show_rainbow(uint32_t now);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

static uint8_t s_frame_buffer[DISPLAY_FRAME_BUFFER_SIZE];
static uint8_t s_physical_buffer[DISPLAY_PHYSICAL_LED_COUNT * 3U];
static uint32_t s_rainbow_last_tick;

static display_ctrl_param_t  s_ctrl;
static display_parse_param_t s_parse;
static display_runtime_t     s_runtime;
uint8_t sport_mode;
uint8_t sport_level;
uint8_t sport_error_code;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int display_init(void)
{
    memset(&s_ctrl, 0, sizeof(s_ctrl));
    memset(&s_parse, 0, sizeof(s_parse));
    memset(&s_runtime, 0, sizeof(s_runtime));

    s_ctrl.brightness       = DISPLAY_DEFAULT_BRIGHTNESS;
    s_ctrl.active_anim_id   = DISPLAY_DEFAULT_ACTIVE_ANIM_ID;
    s_ctrl.frame_interval   = DISPLAY_DEFAULT_FRAME_INTERVAL;
    s_ctrl.play_mode        = DISPLAY_DEFAULT_PLAY_MODE;
    s_ctrl.static_frame_idx = DISPLAY_DEFAULT_STATIC_FRAME_IDX;
    s_ctrl.firmware_version = DISPLAY_DEFAULT_FW_VERSION;
    s_ctrl.hw_error_code    = DISPLAY_DEFAULT_HW_ERROR_CODE;
    s_ctrl.mirror_enabled   = 0;

    s_parse.anim_data       = NULL;
    s_parse.frame_count     = 0;
    s_parse.color_depth     = DISPLAY_COLOR_DEPTH_24BIT;

    s_runtime.frame_index     = 0;
    s_runtime.refresh_pending = false;
    s_runtime.last_frame_tick = 0;
    s_runtime.anim_finished   = false;

    memset(s_frame_buffer, 0, sizeof(s_frame_buffer));
    memset(s_physical_buffer, 0, sizeof(s_physical_buffer));

    int ret = ws2816_init(DISPLAY_PHYSICAL_LED_COUNT);
    if (ret != 0)
    {
        return ret;
    }
    ws2816_set_current_gain(DISPLAY_RAINBOW_GAIN, DISPLAY_RAINBOW_GAIN,
                            DISPLAY_RAINBOW_GAIN);
    ws2816_set_brightness(96U);
    s_rainbow_last_tick = HAL_GetTick();
    display_show_rainbow(s_rainbow_last_tick);

    return 0;
}

void display_deinit(void)
{

    ws2816_clear();
    ws2816_show();
    ws2816_deinit();

    s_parse.anim_data = NULL;
    s_runtime.refresh_pending = false;
    s_runtime.anim_finished = false;
}

/****************************************************************************/
/*						UDS 控制参数写入接口										*/
/*  直接修改 ctrl 并触发对应的显示更新动作                                  */
/****************************************************************************/

void display_set_brightness( uint8_t brightness)
{

    s_ctrl.brightness = brightness;
    ws2816_set_brightness(brightness);
    if (s_ctrl.play_mode != DISPLAY_PLAY_MODE_OFF)
    {
        s_runtime.refresh_pending = true;
    }
}

void display_set_play_mode( uint8_t mode)
{

    s_ctrl.play_mode = mode;

    switch (mode)
    {
        case DISPLAY_PLAY_MODE_OFF:
            s_parse.anim_data = NULL;
            s_runtime.anim_finished = false;
            ws2816_clear();
            ws2816_show();
            break;
        case DISPLAY_PLAY_MODE_STATIC:
            s_runtime.refresh_pending = true;
            break;
        case DISPLAY_PLAY_MODE_LOOP:
        case DISPLAY_PLAY_MODE_SINGLE:
            s_runtime.frame_index = 0;
            s_runtime.anim_finished = false;
            s_runtime.last_frame_tick = HAL_GetTick();
            s_runtime.refresh_pending = true;
            break;
        default:
            break;
    }
}

void display_set_frame_interval( uint16_t interval)
{

    s_ctrl.frame_interval = interval;
}

void display_set_active_anim_id( uint8_t anim_id)
{

    s_ctrl.active_anim_id = anim_id;

    int ret = display_load_from_flash();
    if (ret != 0)
    {
        s_parse.anim_data = NULL;
        s_parse.frame_count = 0;
    }
}

void display_set_static_frame_idx( uint8_t idx)
{

    s_ctrl.static_frame_idx = idx;

    if (s_ctrl.play_mode == DISPLAY_PLAY_MODE_STATIC &&
        idx < s_parse.frame_count)
    {
        s_runtime.frame_index = idx;
        s_runtime.refresh_pending = true;
    }
}

void display_set_mirror( uint8_t enabled)
{

    s_ctrl.mirror_enabled = enabled;
    s_runtime.refresh_pending = true;
}

void display_get_sport_mode_and_level( uint8_t mode, uint8_t level, uint8_t error_code)
{
    sport_mode = mode;
    sport_level = level;
    sport_error_code = error_code;
}

/****************************************************************************/
/*						Frame Decoding (Extensible)						*/
/****************************************************************************/

int display_decode_frame(const uint8_t *src, uint8_t *dst, display_color_depth_t depth)
{
    if (src == NULL || dst == NULL)
    {
        return -1;
    }

    switch (depth)
    {
        case DISPLAY_COLOR_DEPTH_3BIT:
            decode_frame_3bit(src, dst);
            break;
        case DISPLAY_COLOR_DEPTH_24BIT:
            decode_frame_24bit(src, dst);
            break;
        default:
            return -1;
    }

    return 0;
}

/****************************************************************************/
/*						Frame Size Calculation								*/
/****************************************************************************/

uint16_t display_get_frame_bytes(display_color_depth_t depth)
{
    switch (depth)
    {
        case DISPLAY_COLOR_DEPTH_3BIT:
            return (uint16_t)((DISPLAY_LOGICAL_LED_COUNT * 3U + 7U) / 8U);
        case DISPLAY_COLOR_DEPTH_24BIT:
            return (uint16_t)(DISPLAY_LOGICAL_LED_COUNT * 3U);
        default:
            return 0;
    }
}

/****************************************************************************/
/*						Flash Bridge										*/
/*  根据 ctrl.active_anim_id 从 Flash 动画表中读取 header，               */
/*  将 parse.anim_data 指向 Flash 地址空间 (memory-mapped)                  */
/****************************************************************************/

int display_load_from_flash(void)
{
    anim_index_t meta;
    int ret = app_flash_anim_meta_find(s_ctrl.active_anim_id, &meta);
    if (ret != 0)
    {
        s_parse.anim_data = NULL;
        s_parse.frame_count = 0;
        return -2;
    }

    if (meta.frame_count == 0 || meta.valid != 1)
    {
        s_parse.anim_data = NULL;
        s_parse.frame_count = 0;
        return -3;
    }

    if (display_get_frame_bytes((display_color_depth_t)meta.color_depth) == 0)
    {
        s_parse.anim_data = NULL;
        s_parse.frame_count = 0;
        return -4;
    }

    s_parse.anim_data    = (const uint8_t *)meta.addr;
    s_parse.frame_count  = meta.frame_count;
    s_parse.color_depth  = (display_color_depth_t)meta.color_depth;

    s_runtime.frame_index   = 0;
    s_runtime.anim_finished = false;

    return 0;
}

/****************************************************************************/
/*						Animation Load (Non-Flash Path)					*/
/*  用于测试或 RAM 临时数据，不修改 ctrl 字段                               */
/****************************************************************************/

int display_load_animation( const uint8_t *anim_data,
                              uint16_t frame_count, display_color_depth_t depth)
{
    if (anim_data == NULL || frame_count == 0)
    {
        return -1;
    }

    if (display_get_frame_bytes(depth) == 0)
    {
        return -1;
    }

    s_parse.anim_data    = anim_data;
    s_parse.frame_count  = frame_count;
    s_parse.color_depth  = depth;

    s_runtime.frame_index   = 0;
    s_runtime.anim_finished = false;

    return 0;
}

/****************************************************************************/
/*						Static Frame Display								*/
/****************************************************************************/

int display_show_frame( const uint8_t *frame_data,
                         display_color_depth_t depth)
{
    if (frame_data == NULL)
    {
        return -1;
    }

    if (display_decode_frame(frame_data, s_frame_buffer, depth) != 0)
    {
        return -1;
    }

    s_parse.anim_data = NULL;
    s_runtime.refresh_pending = true;

    return 0;
}

/****************************************************************************/
/*						Display Off (Clear Screen)						*/
/*  仅清屏和清除运行时状态，不修改 ctrl                                     */
/****************************************************************************/

void display_off(void)
{

    s_parse.anim_data = NULL;
    s_runtime.anim_finished = false;
    s_runtime.refresh_pending = false;
    ws2816_clear();
    ws2816_show();
}

/****************************************************************************/
/*						Display Task (Polling)									*/
/*  两步流水：1. acquire — 根据参数获取当前帧像素数据                      */
/*            2. send    — 将帧数据发送到 LED                                */
/****************************************************************************/

void display_task(void)
{
    uint32_t now;


    now = HAL_GetTick();
    if (s_ctrl.play_mode == DISPLAY_PLAY_MODE_OFF)
    {
        if ((uint32_t)(now - s_rainbow_last_tick) >= DISPLAY_RAINBOW_FRAME_MS)
        {
            s_rainbow_last_tick = now;
            display_show_rainbow(now);
        }
        return;
    }

    display_acquire_frame();
    display_send_frame();
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static void display_show_rainbow(uint32_t now)
{
    uint8_t start_hue = (uint8_t)(now / DISPLAY_RAINBOW_HUE_MS);

    for (uint16_t i = 0; i < DISPLAY_PHYSICAL_LED_COUNT; i++)
    {
        uint8_t hue8 = (uint8_t)(start_hue + (uint8_t)i);
        uint16_t hue_degrees = (uint16_t)(((uint32_t)hue8 * 360U) / 256U);
        ws2816_value_t red;
        ws2816_value_t green;
        ws2816_value_t blue;

        ws2816_hsv_to_rgb(hue_degrees, 255U, 255U, &red, &green, &blue);
        ws2816_set_pixel(i, red, green, blue);
    }
    ws2816_show();
}

/*
 * display_acquire_frame — Part 1: 根据参数获取当前帧像素数据
 */
static void display_acquire_frame(void)
{
    if (DISPLAY_PLAY_MODE_IS_PLAYING(s_ctrl.play_mode) && s_parse.anim_data != NULL)
    {
        if (!s_runtime.anim_finished)
        {
            uint32_t now = HAL_GetTick();
            uint32_t elapsed = now - s_runtime.last_frame_tick;

            if (elapsed >= s_ctrl.frame_interval)
            {
                s_runtime.frame_index++;
                s_runtime.last_frame_tick = now;

                if (s_runtime.frame_index >= s_parse.frame_count)
                {
                    if (s_ctrl.play_mode == DISPLAY_PLAY_MODE_LOOP)
                    {
                        s_runtime.frame_index = 0;
                    }
                    else
                    {
                        s_runtime.frame_index = s_parse.frame_count - 1;
                        s_runtime.anim_finished = true;
                    }
                }

                s_runtime.refresh_pending = true;
            }
        }
    }

    if (s_runtime.refresh_pending)
    {
        if (s_parse.anim_data != NULL)
        {
            uint16_t idx = s_runtime.frame_index;
            if (idx >= s_parse.frame_count)
            {
                idx = 0;
            }

            uint16_t frame_bytes = display_get_frame_bytes(s_parse.color_depth);
            const uint8_t *frame_ptr = s_parse.anim_data + (uint32_t)idx * frame_bytes;
            display_decode_frame(frame_ptr, s_frame_buffer, s_parse.color_depth);
        }
    }
}

/*
 * display_send_frame - Part 2: 将逻辑帧缓冲发送到物理 LED
 */
static void display_send_frame(void)
{
    if (!s_runtime.refresh_pending)
    {
        return;
    }

    if (s_ctrl.mirror_enabled)
    {
        mirror_frame(s_frame_buffer);
    }

    display_linear_to_physical(s_frame_buffer, s_physical_buffer);

    for (uint16_t i = 0; i < DISPLAY_PHYSICAL_LED_COUNT; i++)
    {
        ws2816_set_pixel(i, s_physical_buffer[i * 3U + 1U],
                            s_physical_buffer[i * 3U],
                            s_physical_buffer[i * 3U + 2U]);
    }

    ws2816_show();
    s_runtime.refresh_pending = false;
}

static uint16_t display_linear_to_physical(const uint8_t *logical_buffer,
                                           uint8_t *physical_buffer)
{
    uint16_t out_idx = 0;

    for (uint16_t row = 0; row < DISPLAY_HEIGHT; row++)
    {
        if ((row & 1) == 0)
        {
            for (uint16_t col = 0; col < DISPLAY_WIDTH; col++)
            {
                if (s_led_mask[row][col])
                {
                    uint16_t src = (row * DISPLAY_WIDTH + col) * 3;
                    physical_buffer[out_idx++] = logical_buffer[src];
                    physical_buffer[out_idx++] = logical_buffer[src + 1U];
                    physical_buffer[out_idx++] = logical_buffer[src + 2U];
                }
            }
        }
        else
        {
            for (int16_t col = DISPLAY_WIDTH - 1; col >= 0; col--)
            {
                if (s_led_mask[row][col])
                {
                    uint16_t src = (row * DISPLAY_WIDTH + (uint16_t)col) * 3;
                    physical_buffer[out_idx++] = logical_buffer[src];
                    physical_buffer[out_idx++] = logical_buffer[src + 1U];
                    physical_buffer[out_idx++] = logical_buffer[src + 2U];
                }
            }
        }
    }

    return out_idx;
}

static void decode_frame_3bit(const uint8_t *packed, uint8_t *grb_buf)
{
    uint16_t pixel = 0;
    uint16_t bit_idx = 0;

    for (uint16_t row = 0; row < DISPLAY_HEIGHT; row++)
    {
        for (uint16_t col = 0; col < DISPLAY_WIDTH; col++)
        {
            uint8_t byte_idx = bit_idx / 8;
            uint8_t bit_pos = 7 - (bit_idx % 8);

            uint8_t r = (packed[byte_idx] >> bit_pos) & 0x01 ? 255 : 0;

            bit_idx++;
            byte_idx = bit_idx / 8;
            bit_pos = 7 - (bit_idx % 8);

            uint8_t g = (packed[byte_idx] >> bit_pos) & 0x01 ? 255 : 0;

            bit_idx++;
            byte_idx = bit_idx / 8;
            bit_pos = 7 - (bit_idx % 8);

            uint8_t b = (packed[byte_idx] >> bit_pos) & 0x01 ? 255 : 0;

            bit_idx++;

            grb_buf[pixel * 3 + 0] = g;
            grb_buf[pixel * 3 + 1] = r;
            grb_buf[pixel * 3 + 2] = b;

            pixel++;
        }
    }
}

static void decode_frame_24bit(const uint8_t *rgb_buf, uint8_t *grb_buf)
{
    for (uint16_t i = 0; i < DISPLAY_LOGICAL_LED_COUNT; i++)
    {
        grb_buf[i * 3 + 0] = rgb_buf[i * 3 + 1];
        grb_buf[i * 3 + 1] = rgb_buf[i * 3 + 0];
        grb_buf[i * 3 + 2] = rgb_buf[i * 3 + 2];
    }
}

static void mirror_frame(uint8_t *grb_buf)
{
    for (uint16_t row = 0; row < DISPLAY_HEIGHT; row++)
    {
        for (uint16_t col = 0; col < DISPLAY_WIDTH / 2; col++)
        {
            uint16_t left = (row * DISPLAY_WIDTH + col) * 3;
            uint16_t right = (row * DISPLAY_WIDTH + (DISPLAY_WIDTH - 1 - col)) * 3;

            uint8_t tmp;
            tmp = grb_buf[left + 0];
            grb_buf[left + 0] = grb_buf[right + 0];
            grb_buf[right + 0] = tmp;

            tmp = grb_buf[left + 1];
            grb_buf[left + 1] = grb_buf[right + 1];
            grb_buf[right + 1] = tmp;

            tmp = grb_buf[left + 2];
            grb_buf[left + 2] = grb_buf[right + 2];
            grb_buf[right + 2] = tmp;
        }
    }
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
