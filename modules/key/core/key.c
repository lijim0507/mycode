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

/*  core 层内部接口（在各自 .c 中实现）                                     */
int  key_filter_process(key_t *key);
void key_state_process(key_t *key);
void key_event_dispatch(key_t *key, key_event_t event);

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化按键对象
 */
void key_init(key_t *key)
{
    if (!key) {
        return;
    }

    key->read                = NULL;
    key->get_tick            = NULL;
    key->callback            = NULL;

    /* 默认配置 */
    key->debounce_ms         = 10;
    key->long_press_ms       = 1000;
    key->click_timeout_ms    = 300;
    key->repeat_interval_ms  = 0;

    /* 清零运行时状态 */
    key->raw_level           = 0;
    key->stable_level        = 0;
    key->last_stable_level   = 0;
    key->debounce_active     = 0;
    key->debounce_tick       = 0;
    key->state               = KEY_ST_IDLE;
    key->long_triggered      = 0;
    key->press_tick          = 0;
    key->click_count         = 0;
    key->click_timeout_tick  = 0;
    key->last_repeat_tick    = 0;
}

/**
 * @brief  注册按键事件回调
 */
void key_register_callback(key_t *key, key_callback_t cb)
{
    if (key) {
        key->callback = cb;
    }
}

/**
 * @brief  按键扫描（非阻塞）
 * @note   流程：read → filter → state → event
 *         filter 处理消抖，state 处理状态机与时间事件，
 *         state 每次扫描都运行（不仅电平变化时），以检测长按/双击/连击超时
 */
void key_scan(key_t *key)
{
    if (!key || !key->read || !key->get_tick) {
        return;
    }

    key_filter_process(key);
    key_state_process(key);
}

/**
 * @brief  批量按键扫描
 */
void key_manager_scan(key_manager_t *mgr)
{
    if (!mgr || !mgr->keys) {
        return;
    }

    for (uint32_t i = 0; i < mgr->num; i++) {
        key_scan(&mgr->keys[i]);
    }
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
