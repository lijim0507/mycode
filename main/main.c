/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "main.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/
void app_main(void)
{
    printf("Hello world!\n");
}



//----------------------------------------------------------------------------
//二值信号量句柄--demo
osSemaphoreId_t demo_semaphore_handle;

/**
 * @brief 二值信号量操作封装示例
 * @param op 操作类型，取值为SEMAPHORE_OP_GIVE或SEMAPHORE_OP_TAKE
 * 
 */
uint8_t demo_semaphore_operations(semaphore_operation_t op) 
{
    /* Local variables */
    uint8_t rc = 0;

    switch (op) 
    {
        case SEMAPHORE_OP_GIVE:
            rc = xSemaphoreGive(demo_semaphore_handle);
            if (rc != pdTRUE) 
            {
                ESP_LOGE(TAG, "failed to give semaphore");
                return -2;
            }
            break;

        case SEMAPHORE_OP_TAKE:
            rc = xSemaphoreTake(demo_semaphore_handle, portMAX_DELAY);
            if (rc != pdTRUE) 
            {
                ESP_LOGE(TAG, "failed to take semaphore");
                return -3;
            }
            break;

        case SEMAPHORE_OP_NULL:
        default:
            ESP_LOGE(TAG, "invalid semaphore operation");
            return -4;
    }
    return 0;
}


//----------------------------------------------------------------------------

/* CPU 占用率 + 任务状态 (由 app_calc_cpu_usage() 每 100ms 刷新) */
#define TOTAL_TASK_NUMBER   16

typedef struct _task_status_t
{
    char   name[configMAX_TASK_NAME_LEN];  /* 任务名 */
    float  cpu_usage;                      /* CPU 占用率 (%) */
    eTaskState state;                      /* 任务状态 */
    UBaseType_t priority;                  /* 当前优先级 */
    uint16_t stack_watermark;              /* 栈高水位 (字) */
} task_status_t;

static task_status_t g_task_status[TOTAL_TASK_NUMBER];
static UBaseType_t   g_task_count;  /* 当前有效任务数（供外部读取 g_task_status 边界） */

/** @brief 上一轮快照：关联 task_number 与 run_time */
typedef struct {
    UBaseType_t task_number;
    uint32_t    run_time;
} prev_snapshot_t;

/* 初始化RUN TIME的TIMER */
__weak void configureTimerForRunTimeStats(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* 获取RUN TIME */
__weak unsigned long getRunTimeCounterValue(void)
{
    return DWT->CYCCNT;
}


void calc_cpu_usage(void)
{
    static uint32_t prev_run_time[16] = {0};
    static uint32_t prev_total_time = 0;
    static UBaseType_t prev_task_count = 0;
    static UBaseType_t prev_task_numbers[16] = {0};

    TaskStatus_t arr[16];
    uint32_t total;
    UBaseType_t count = uxTaskGetSystemState(arr, 16, &total);

    /* 按 xTaskNumber 排序 */
    for (UBaseType_t i = 0; i + 1 < count; i++)
    {
        for (UBaseType_t j = i + 1; j < count; j++)
        {
            if (arr[i].xTaskNumber > arr[j].xTaskNumber)
            {
                TaskStatus_t t = arr[i];
                arr[i] = arr[j];
                arr[j] = t;
            }
        }
    }

    /* 首次调用仅保存快照，不计算 cpu_usage */
    uint32_t delta_total = (prev_task_count > 0U) ? (total - prev_total_time) : 0U;

    for (UBaseType_t i = 0; i < count; i++)
    {
        task_status_t *p = &g_task_status[i];

        strncpy(p->name, arr[i].pcTaskName, configMAX_TASK_NAME_LEN - 1);
        p->name[configMAX_TASK_NAME_LEN - 1] = '\0';

        p->state   = arr[i].eCurrentState;
        p->priority = arr[i].uxCurrentPriority;
        p->stack_watermark = arr[i].usStackHighWaterMark;

        /* 按 xTaskNumber 匹配上一轮数据，计算增量 */
        uint32_t prev_val = 0U;
        uint32_t task_num = arr[i].xTaskNumber;
        for (UBaseType_t k = 0; k < prev_task_count; k++)
        {
            if (prev_task_numbers[k] == task_num)
            {
                prev_val = prev_run_time[k];
                break;
            }
        }

        if (delta_total > 0U)
        {
            uint32_t delta_task = arr[i].ulRunTimeCounter - prev_val;
            p->cpu_usage = (float)delta_task * 100.0f / (float)delta_total;
        }
        else
        {
            p->cpu_usage = 0.0f;
        }
    }

    /* 保存本轮快照 */
    prev_total_time = total;
    prev_task_count = count;
    for (UBaseType_t i = 0; i < count; i++)
    {
        prev_run_time[i] = arr[i].ulRunTimeCounter;
        prev_task_numbers[i] = arr[i].xTaskNumber;
    }
}


/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/

