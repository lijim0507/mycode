/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "sys_manager.h"

#include <stddef.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/
typedef struct 
{
    sys_state_t sys_state;
    sys_error_mask_t error_mask;
} sys_manager_ctx_t;


/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static void sys_manager_overcurrent_error(sys_error_t error, int8_t detail);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/


/*
 * 错误处理表 —— 用户按需修改
 *
 * 格式: { 错误位, 处理函数 }
 * 处理函数需在表前可见（头文件声明或前置原型）
 */
static const struct
{
    sys_error_t                 error;
    sys_manager_error_handler_t handler;
} s_error_table[] =
{
    { SYS_ERR_OVERCURRENT, sys_manager_overcurrent_error },
    // { SYS_ERR_OVERTEMP,    NULL },
    // { SYS_ERR_COMM_LOST,   NULL },
    // { SYS_ERR_SENSOR,      NULL },
};


sys_manager_ctx_t g_sys_manager_ctx = {0};

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief 上报错误
 *
 * @param error 单个错误位
 * @param detail 该错误的附加信息
 */
void sys_manager_report_error(sys_error_t error, int8_t detail)
{
    size_t count;
    size_t i;

    if (error == SYS_ERR_NONE)
    {
        return;
    }

    if ((g_sys_manager_ctx.error_mask & (sys_error_mask_t)error) != 0)
    {
        return;
    }

    g_sys_manager_ctx.error_mask |= (sys_error_mask_t)error;

    count = sizeof(s_error_table) / sizeof(s_error_table[0]);

    for (i = 0; i < count; i++)
    {
        if (s_error_table[i].error == error)
        {
            if (s_error_table[i].handler != NULL)
            {
                s_error_table[i].handler(error, detail);
            }
            return;
        }
    }
}

/**
 * @brief 清除错误位
 *
 * @param error 要清除的错误位或组合
 */
void sys_manager_clear_error(sys_error_mask_t error)
{
    g_sys_manager_ctx.error_mask &= ~error;
}

/**
 * @brief 清除全部错误位
 */
void sys_manager_clear_all_errors(void)
{
    g_sys_manager_ctx.error_mask = 0;
}

/**
 * @brief 获取当前错误位
 *
 * @return 当前错误掩码
 */
sys_error_mask_t sys_manager_get_error_mask(void)
{
    return g_sys_manager_ctx.error_mask;
}

/**
 * @brief 查询错误位是否置位
 *
 * @param error 错误位或组合，任意一位匹配返回 true
 * @return true 置位，false 未置位
 */
bool sys_manager_has_error(sys_error_mask_t error)
{
    return (g_sys_manager_ctx.error_mask & error) != 0;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief 过流错误处理
 *
 * @param error 错误位
 * @param detail 错误附加信息
 */
static void sys_manager_overcurrent_error(sys_error_t error, int8_t detail)
{
    (void)error;
    (void)detail;

    /* 在这里执行过流保护动作 */
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
