/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "fct.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/
#define APP_TAG             "APP"

#define APP_FCT_TASK_STACK  4096        /* FCT 模拟任务栈大小 */
#define APP_FCT_TASK_PRIO   5           /* FCT 模拟任务优先级 */

#define APP_HB_PERIOD_MS    2000        /* 心跳周期，与协议约定一致 */
#define APP_ROUND_PERIOD_MS 10000       /* 两轮模拟测试之间的空闲时间 */
#define APP_LOG_GAP_MS      300         /* 模拟测试中两条日志之间的间隔 */
#define APP_FCT_NG_CODE     12U         /* 模拟 NG 时上报的错误码，含义由现场定义 */

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/
static void app_fct_run_round(uint32_t round);
static void app_fct_demo_task(void *param);

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief 应用入口
 */
void app_main(void)
{
    ESP_LOGI(APP_TAG, "esp32s3_demo start");

    if (xTaskCreate(app_fct_demo_task, "fct_demo", APP_FCT_TASK_STACK,
                    NULL, APP_FCT_TASK_PRIO, NULL) != pdPASS)
    {
        ESP_LOGE(APP_TAG, "create fct demo task failed");
    }
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief 模拟一轮 FCT 测试，奇偶轮交替输出 OK 和 NG
 *
 * @param round 轮次，从 1 开始
 */
static void app_fct_run_round(uint32_t round)
{
    bool ng = ((round % 2U) == 0U);
    int  ret;

    ret = fct_start();
    if (ret != 0)
    {
        ESP_LOGE(APP_TAG, "fct_start failed: %d", ret);
        return;
    }

    fct_logf(FCT_LOG_INFO, "round %lu: power on self test start",
             (unsigned long)round);
    vTaskDelay(pdMS_TO_TICKS(APP_LOG_GAP_MS));

    fct_logf(FCT_LOG_INFO, "measure 3.3V = 3.31");
    vTaskDelay(pdMS_TO_TICKS(APP_LOG_GAP_MS));

    fct_logf(FCT_LOG_DEBUG, "ambient temperature = 26.5 C");
    vTaskDelay(pdMS_TO_TICKS(APP_LOG_GAP_MS));

    if (ng)
    {
        fct_logf(FCT_LOG_WARN, "measure 5V = 4.90, near limit");
        vTaskDelay(pdMS_TO_TICKS(APP_LOG_GAP_MS));

        fct_logf(FCT_LOG_ERROR, "measure 5V = 4.62, out of range");
    }
    else
    {
        fct_logf(FCT_LOG_INFO, "measure 5V = 5.02");
    }
    vTaskDelay(pdMS_TO_TICKS(APP_LOG_GAP_MS));

    ret = fct_result(ng ? FCT_RESULT_NG : FCT_RESULT_OK,
                     ng ? APP_FCT_NG_CODE : FCT_ERROR_CODE_NONE);
    if (ret != 0)
    {
        ESP_LOGE(APP_TAG, "fct_result failed: %d", ret);
    }

    ret = fct_end();
    if (ret != 0)
    {
        ESP_LOGE(APP_TAG, "fct_end failed: %d", ret);
    }

    ESP_LOGI(APP_TAG, "round %lu done: %s", (unsigned long)round,
             ng ? "NG" : "OK");
}

/**
 * @brief FCT 模拟测试任务
 *
 * 空闲时每 APP_HB_PERIOD_MS 发送一条 IDLE 心跳，每 APP_ROUND_PERIOD_MS
 * 执行一轮模拟测试，用于验证上位机的接收、CSV 记录和界面显示。
 *
 * @param param 未使用
 */
static void app_fct_demo_task(void *param)
{
    uint32_t elapsed_ms = 0;
    uint32_t round = 0;
    int      ret;

    (void)param;

    ret = fct_init();
    if (ret != 0)
    {
        ESP_LOGE(APP_TAG, "fct_init failed: %d", ret);
        vTaskDelete(NULL);
        return;
    }

    for (;;)
    {
        /* 心跳每 2 秒一条，测试期间由模块自动切换为 TESTING */
        fct_hb();
        vTaskDelay(pdMS_TO_TICKS(APP_HB_PERIOD_MS));
        elapsed_ms += APP_HB_PERIOD_MS;

        if (elapsed_ms < APP_ROUND_PERIOD_MS)
        {
            continue;
        }

        elapsed_ms = 0;
        round++;
        app_fct_run_round(round);
    }
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
