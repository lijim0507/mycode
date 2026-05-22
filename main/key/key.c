/****************************************************************************/
/*							Includes									*/
/****************************************************************************/
#include "key.h"
/****************************************************************************/
/*							Macros										*/
/****************************************************************************/
/* 滤波阈值：连续检测到相同状态的次数，推荐在 5~10ms 周期下使用 2~4 次 */
#define KEY_FILTER_CNT      3

/****************************************************************************/
/*							Typedefs										*/
/****************************************************************************/

/****************************************************************************/
/*					Prototypes Of Local Functions						*/
/****************************************************************************/
static uint8_t key_is_active(uint8_t index);

/****************************************************************************/
/*						Global Variables								*/
/****************************************************************************/
static uint8_t s_key_filter_cnt[KEY_NUM];
static uint8_t s_key_state[KEY_NUM];        /* 0: 释放, 1: 按下 */
static key_event_t s_key_event[KEY_NUM];

/****************************************************************************/
/*						Exported Functions    							*/
/****************************************************************************/

/**
 * @brief  按键初始化
 * @note   清空滤波状态与事件缓冲区，请在 GPIO 初始化完成后调用
 */
void key_init(void)
{
    for (uint8_t i = 0; i < KEY_NUM; i++)
    {
        s_key_filter_cnt[i] = 0;
        s_key_state[i]      = 0;
        s_key_event[i]      = KEY_EVENT_NONE;
    }
}

/**
 * @brief  按键扫描，周期性调用以实现去抖滤波
 * @note   推荐调用周期 5~10ms
 */
void key_scan(void)
{
    for (uint8_t i = 0; i < KEY_NUM; i++)
    {
        uint8_t active = key_is_active(i);

        if (active)
        {
            /* 检测到按下信号，滤波计数器递增 */
            if (s_key_filter_cnt[i] < KEY_FILTER_CNT)
            {
                s_key_filter_cnt[i]++;
                if (s_key_filter_cnt[i] == KEY_FILTER_CNT && s_key_state[i] == 0)
                {
                    s_key_state[i] = 1;
                    s_key_event[i] = KEY_EVENT_PRESSED;
                }
            }
        }
        else
        {
            /* 检测到释放信号，滤波计数器递减 */
            if (s_key_filter_cnt[i] > 0)
            {
                s_key_filter_cnt[i]--;
                if (s_key_filter_cnt[i] == 0 && s_key_state[i] == 1)
                {
                    s_key_state[i] = 0;
                    s_key_event[i] = KEY_EVENT_RELEASED;
                }
            }
        }
    }
}

/**
 * @brief  获取指定按键的一次性事件
 * @param  index  按键索引
 * @return key_event_t  按键事件，读取后内部事件自动清零
 */
key_event_t key_get_event(uint8_t index)
{
    key_event_t evt;

    if (index >= KEY_NUM)
    {
        return KEY_EVENT_NONE;
    }

    evt = s_key_event[index];
    s_key_event[index] = KEY_EVENT_NONE;

    return evt;
}

/**
 * @brief  查询指定按键当前是否处于按下状态
 * @param  index  按键索引
 * @return uint8_t  1: 按下, 0: 释放
 */
uint8_t key_is_pressed(uint8_t index)
{
    if (index >= KEY_NUM)
    {
        return 0;
    }

    return s_key_state[index];
}

/****************************************************************************/
/*						Static Functions    							*/
/****************************************************************************/

/**
 * @brief  读取指定按键是否处于有效按下状态
 * @param  index  按键索引
 * @return uint8_t  1: 按下有效, 0: 未按下
 */
static uint8_t key_is_active(uint8_t index)
{
    uint8_t raw = 0;

    switch (index)
    {
        case KEY_IDX_0:
            raw = KEY0_GPIO_READ();
            return (raw == KEY0_ACTIVE_LEVEL) ? 1 : 0;

        case KEY_IDX_1:
            raw = KEY1_GPIO_READ();
            return (raw == KEY1_ACTIVE_LEVEL) ? 1 : 0;

        default:
            return 0;
    }
}

/****************************************************************************/
/*							EOF											*/
/****************************************************************************/
