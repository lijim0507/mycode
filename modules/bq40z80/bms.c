/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "app_bms.h"

#include "app_bq40z80.h"
#include "app_rtos.h"
#include "esp_log.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

static const char *TAG = "APP_BMS";

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static void app_bms_read_all(void);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static bms_data_t g_bms_data = {0};

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 BMS 模块
 */
void app_bms_init(void)
{
    int ret = bq40z80_init();
    if (ret != 0)
    {
        ESP_LOGE(TAG, "bq40z80_init failed: %d", ret);
        return;
    }

    /* 启动后立即执行一次读取，填充初始缓存 */
    app_bms_read_all();

    ESP_LOGW(TAG, "BMS init done, voltage=%umV current=%dmA soc=%u%% soh=%u%% valid=%d",
             g_bms_data.voltage_mv, g_bms_data.current_ma,
             g_bms_data.soc_pc, g_bms_data.soh_pc, g_bms_data.valid);
}

/**
 * @brief  BMS 周期读取任务入口
 */
void app_bms_task_handler(void *pvParameters)
{
    while (app_rtos_bms_sema_take())
    {
        app_bms_read_all();

        if (g_bms_data.valid)
        {
            ESP_LOGW(TAG, "V=%umV I=%dmA T=%.1fC SOC=%u%% SOH=%u%% Status=0x%04X",
                     g_bms_data.voltage_mv,
                     g_bms_data.current_ma,
                     (float)g_bms_data.temperature_0_1k / 10.0f - 273.15f,
                     g_bms_data.soc_pc,
                     g_bms_data.soh_pc,
                     g_bms_data.battery_status);
        }
    }
}

/**
 * @brief  获取 BMS 最新监测数据
 */
const bms_data_t *app_bms_get_data(void)
{
    return &g_bms_data;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  读取全部监测寄存器并更新内部缓存
 * @note   单次读取失败不置 invalid，保留上一轮有效数据继续使用。
 *         仅在全部关键寄存器读取成功后才标记 valid。
 */
static void app_bms_read_all(void)
{
    uint16_t voltage_mv       = 0;
    int16_t  current_ma       = 0;
    uint16_t temp_0_1k        = 0;
    uint8_t  soc_pc           = 0;
    uint16_t fcc_mah          = 0;
    uint16_t design_cap_mah   = 0;
    uint16_t battery_status   = 0;

    bool voltage_ok  = (bq40z80_get_voltage(&voltage_mv) == 0);
    bool current_ok  = (bq40z80_get_current(&current_ma) == 0);
    bool temp_ok     = (bq40z80_get_temperature(&temp_0_1k) == 0);
    bool soc_ok      = (bq40z80_get_relative_state_of_charge(&soc_pc) == 0);
    bool fcc_ok      = (bq40z80_get_full_charge_capacity(&fcc_mah) == 0);
    bool design_ok   = (bq40z80_get_design_capacity(&design_cap_mah) == 0);
    bool status_ok   = (bq40z80_get_battery_status(&battery_status) == 0);

    /* 更新缓存 — 单寄存器读取成功即更新对应字段 */
    if (voltage_ok)
    {
        g_bms_data.voltage_mv = voltage_mv;
    }
    if (current_ok)
    {
        g_bms_data.current_ma = current_ma;
    }
    if (temp_ok)
    {
        g_bms_data.temperature_0_1k = temp_0_1k;
    }
    if (soc_ok)
    {
        g_bms_data.soc_pc = soc_pc;
    }
    if (status_ok)
    {
        g_bms_data.battery_status = battery_status;
    }

    /* SOH = FCC / 设计容量 × 100%，两者均成功才更新 */
    if (fcc_ok && design_ok && design_cap_mah > 0)
    {
        g_bms_data.soh_pc = (uint8_t)((uint32_t)fcc_mah * 100U / design_cap_mah);
    }

    /* 全部关键寄存器成功 → 标记有效 */
    g_bms_data.valid = voltage_ok && current_ok && temp_ok && soc_ok;

    if (!g_bms_data.valid)
    {
        ESP_LOGW(TAG, "read partial: V=%d I=%d T=%d SOC=%d FCC=%d Design=%d Status=%d",
                 voltage_ok, current_ok, temp_ok, soc_ok, fcc_ok, design_ok, status_ok);
    }
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
