/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "key.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static void key_state_enter_press(key_t *key, uint32_t tick);
static void key_state_enter_release(key_t *key, uint32_t tick);
static void key_state_check_long_press(key_t *key, uint32_t tick);
static void key_state_check_repeat(key_t *key, uint32_t tick);
static void key_state_check_click_timeout(key_t *key, uint32_t tick);

/****************************************************************************/
/*                          External Declarations                           */
/****************************************************************************/

void key_event_dispatch(key_t *key, key_event_t event);

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  状态机处理（在稳定电平变化时调用）
 * @param  key 按键对象指针
 * @note   同时处理时间驱动事件（长按、连击、双击超时），因此每次 key_scan()
 *         都会调用（即使 level 未变化）。
 */
void key_state_process(key_t *key)
{
    uint32_t tick = key->get_tick();

    /* 边沿检测：稳定电平发生变化 */
    if (key->stable_level != key->last_stable_level) {

        key->last_stable_level = key->stable_level;

        if (key->stable_level) {
            key_state_enter_press(key, tick);
        } else {
            key_state_enter_release(key, tick);
        }
    }

    /* 时间驱动：长按检测 */
    key_state_check_long_press(key, tick);

    /* 时间驱动：连击检测 */
    key_state_check_repeat(key, tick);

    /* 时间驱动：双击超时检测 */
    key_state_check_click_timeout(key, tick);
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  进入按下状态
 */
static void key_state_enter_press(key_t *key, uint32_t tick)
{
    key->state          = KEY_ST_PRESSED;
    key->long_triggered = 0;
    key->press_tick     = tick;
    key->last_repeat_tick = tick;

    /* 双击计数：若前次释放仍在超时窗口内，递加 */
    /* （click_count 在 release 时已加，此处仅检查是否需要 reset） */

    key_event_dispatch(key, KEY_EVENT_PRESS);
}

/**
 * @brief  进入释放状态
 */
static void key_state_enter_release(key_t *key, uint32_t tick)
{
    if (!key->long_triggered) {
        /* 短按释放：累积 click 计数并启动双击超时 */
        key->click_count++;
        key->click_timeout_tick = tick;
    }

    key->state = KEY_ST_IDLE;
    key_event_dispatch(key, KEY_EVENT_RELEASE);
}

/**
 * @brief  检查长按超时
 */
static void key_state_check_long_press(key_t *key, uint32_t tick)
{
    if (key->state != KEY_ST_PRESSED) {
        return;
    }

    if (key->long_triggered) {
        return;
    }

    if (tick - key->press_tick >= key->long_press_ms) {
        key->long_triggered   = 1;
        key->state            = KEY_ST_LONG_PRESS;
        key->click_count      = 0;          /* 长按取消单击/双击 */
        key->last_repeat_tick = tick;        /* 首次连击从长按触发时刻算起 */
        key_event_dispatch(key, KEY_EVENT_LONG_PRESS);
    }
}

/**
 * @brief  检查连击间隔
 */
static void key_state_check_repeat(key_t *key, uint32_t tick)
{
    if (key->state != KEY_ST_LONG_PRESS) {
        return;
    }

    if (key->repeat_interval_ms == 0) {
        return;
    }

    if (tick - key->last_repeat_tick >= key->repeat_interval_ms) {
        key->last_repeat_tick = tick;
        key_event_dispatch(key, KEY_EVENT_REPEAT);
    }
}

/**
 * @brief  检查双击超时
 */
static void key_state_check_click_timeout(key_t *key, uint32_t tick)
{
    if (key->click_count == 0) {
        return;
    }

    if (key->state != KEY_ST_IDLE) {
        return;
    }

    if (tick - key->click_timeout_tick >= key->click_timeout_ms) {

        if (key->click_count == 1) {
            key_event_dispatch(key, KEY_EVENT_CLICK);
        } else if (key->click_count >= 2) {
            key_event_dispatch(key, KEY_EVENT_DOUBLE_CLICK);
        }

        key->click_count = 0;
    }
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
