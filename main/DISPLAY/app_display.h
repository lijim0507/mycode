#ifndef __APP_DISPLAY_H_
#define __APP_DISPLAY_H_

#include <stdint.h>
#include <stdbool.h>
#include "ws2816.h"


#define DISPLAY_WIDTH           16
#define DISPLAY_HEIGHT          16
#define DISPLAY_LOGICAL_LED_COUNT   (DISPLAY_WIDTH * DISPLAY_HEIGHT)
#define DISPLAY_PHYSICAL_LED_COUNT  WS2816_LED_COUNT
#define DISPLAY_FRAME_BUFFER_SIZE   (DISPLAY_LOGICAL_LED_COUNT * 3U)

#define DISPLAY_DEFAULT_INTERVAL_MS  200

#define DISPLAY_SIDE_GPIO_PORT   GPIOA
#define DISPLAY_SIDE_GPIO_PIN    GPIO_PIN_0

/****************************************************************************/
/*						Play Mode Definitions								*/
/****************************************************************************/

#define DISPLAY_PLAY_MODE_OFF      0xFF
#define DISPLAY_PLAY_MODE_STATIC   0x00
#define DISPLAY_PLAY_MODE_LOOP     0x01
#define DISPLAY_PLAY_MODE_SINGLE   0x02

#define DISPLAY_PLAY_MODE_IS_PLAYING(m) \
    ((m) == DISPLAY_PLAY_MODE_LOOP || (m) == DISPLAY_PLAY_MODE_SINGLE)

/****************************************************************************/
/*						Color Depth Enum									*/
/*  解析参数枚举，定义不同的 bit 解析方式                                    */
/*  新增色深时在此枚举中添加值，并在 display_decode_frame 和                */
/*  display_get_frame_bytes 中添加对应的解析分支                            */
/****************************************************************************/

typedef enum {
    DISPLAY_COLOR_DEPTH_3BIT  = 3,
    DISPLAY_COLOR_DEPTH_24BIT = 24,
} display_color_depth_t;

/****************************************************************************/
/*						DID Default Values									*/
/****************************************************************************/

#define DISPLAY_DEFAULT_BRIGHTNESS       128
#define DISPLAY_DEFAULT_ACTIVE_ANIM_ID   1
#define DISPLAY_DEFAULT_FRAME_INTERVAL   200
#define DISPLAY_DEFAULT_PLAY_MODE        DISPLAY_PLAY_MODE_OFF
#define DISPLAY_DEFAULT_STATIC_FRAME_IDX 0
#define DISPLAY_DEFAULT_FW_VERSION       0x0100
#define DISPLAY_DEFAULT_HW_ERROR_CODE    0x00

/****************************************************************************/
/*						Control Parameter (UDS R/W)							*/
/*  控制参数：仅由 UDS/CAN 写入，display 内部只读                            */
/*  初始化时从 Flash 加载，运行时通过 CAN 修改                              */
/****************************************************************************/

typedef struct {
    uint8_t    brightness;         /* DID 0x0100, 1B, R/W, 默认128      */
    uint8_t    active_anim_id;     /* DID 0x0101, 1B, R/W, 默认1        */
    uint16_t   frame_interval;     /* DID 0x0102, 2B, R/W, 默认200ms    */
    uint8_t    play_mode;          /* DID 0x0103, 1B, R/W, 默认OFF      */
    uint8_t    static_frame_idx;   /* DID 0x0105, 1B, R/W, 默认0        */
    uint8_t    mirror_enabled;     /* 镜像使能                            */
    uint16_t   firmware_version;   /* DID 0xF100, 2B, R                  */
    uint8_t    hw_error_code;      /* DID 0xF101, 1B, R, 默认0x00        */
} display_ctrl_param_t;

/****************************************************************************/
/*						Parse Parameter (Animation Metadata)				*/
/*  解析参数：由 display_task 根据 active_anim_id 从 Flash 加载             */
/*  anim_data 指向 Flash 地址空间 (memory-mapped)                          */
/****************************************************************************/

typedef struct 
{
    const uint8_t           *anim_data;
    uint16_t                frame_count;
    display_color_depth_t   color_depth;
} display_parse_param_t;

/****************************************************************************/
/*						Runtime State (Display Internal)					*/
/*  运行时状态：display 模块内部使用，不可由外部直接修改                    */
/****************************************************************************/

typedef struct {
    uint16_t    frame_index;
    bool        refresh_pending;
    uint32_t    last_frame_tick;
    bool        anim_finished;
} display_runtime_t;

/****************************************************************************/
/*						Display Handle											*/
/****************************************************************************/


/****************************************************************************/
/*						Exported Functions									*/
/****************************************************************************/

int  display_init(void);
void display_deinit(void);

/* UDS 控制参数写入接口（直接更新显示状态）*/
void display_set_brightness( uint8_t brightness);
void display_set_play_mode( uint8_t mode);
void display_set_frame_interval( uint16_t interval);
void display_set_active_anim_id( uint8_t anim_id);
void display_set_static_frame_idx( uint8_t idx);
void display_set_mirror( uint8_t enabled);


void display_get_sport_mode_and_level( uint8_t mode, uint8_t level, uint8_t error_code);

/* 帧解码 (通过 display_color_depth_t 枚举扩展) */
int  display_decode_frame(const uint8_t *src, uint8_t *dst, display_color_depth_t depth);
uint16_t display_get_frame_bytes(display_color_depth_t depth);

/* Flash 桥接：根据 active_anim_id 从 Flash 加载动画解析参数 */
int  display_load_from_flash(void);

/* 动画加载（非 Flash 路径，如测试或 RAM 临时数据）*/
int  display_load_animation( const uint8_t *anim_data,
                              uint16_t frame_count, display_color_depth_t depth);

/* 单帧显示（STATIC 模式专用）*/
int  display_show_frame( const uint8_t *frame_data,
                         display_color_depth_t depth);

/* 关闭显示（清屏，不修改 ctrl）*/
void display_off(void);

/* 主循环任务：acquire → send */
void display_task(void);

#endif
