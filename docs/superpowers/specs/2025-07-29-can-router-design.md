# CAN 总管路由模块设计

日期：2025-07-29
状态：已批准

## 概述

新建 `modules/can/`，作为 CAN 总线的唯一所有者。采用标准三层结构（include/core/port），
硬件相关逻辑全部封入 port 层。`can_port_driver_t` 是唯一的 CAN 硬件抽象接口——
isotp、uds 等上层协议统一通过它访问 CAN，不再各自定义驱动类型。

## 目标结构

```
modules/can/
├── include/
│   └── can_bus.h                        ← can_port_driver_t + can_bus_handler_t + 公开 API
├── core/
│   └── can_bus.c                        ← 路由表 + 分发 + 接收任务（平台无关）
├── port/
│   ├── can_bus_port.h                   ← can_bus_port_get_driver()
│   └── esp32_can_bus_port.c             ← 实现 can_port_driver_t（TWAI 操作 + FreeRTOS 任务）
└── can_diag/                            ← 从 modules/can_diag/ 迁入
    ├── isotp/
    │   ├── include/isotp.h              ← #include "can_bus.h"，使用 can_port_driver_t
    │   ├── core/isotp.c                 ← g_isotp_driver 类型改为 const can_port_driver_t *
    │   └── port/                        ← 删除（esp32_isotp_port.c、stm32_isotp_port.c）
    └── uds/
        ├── include/uds.h
        ├── core/uds.c
        └── port/
            ├── uds_port.h
            └── esp32_uds_port.c         ← 只提供 uds_port_config_t（CAN ID），不变
```

## `can_bus.h` — 公开接口

```c
#ifndef __CAN_BUS_H_
#define __CAN_BUS_H_

#include <stdint.h>

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef void (*can_bus_handler_t)(uint32_t id, const uint8_t *data, uint8_t len);

typedef struct can_port_driver
{
    int      (*init)(void);
    int      (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    int      (*receive)(uint32_t *id, uint8_t *data, uint8_t *len);
    int      (*task_create)(void (*task)(void *), void *arg);
    uint32_t (*get_ms)(void);
    void     (*debug)(const char *message, ...);
} can_port_driver_t;

/****************************************************************************/
/*                         Exported Functions                               */
/****************************************************************************/

void can_bus_init(void);
int  can_bus_send(uint32_t id, const uint8_t *data, uint8_t len);

#endif
```

## `can_bus.c` — 实现（平台无关）

core 层不包含任何平台相关代码。所有硬件操作通过 `g_can_driver` 完成，
FreeRTOS 依赖也通过 `driver->task_create` / `driver->get_ms` 间接化。

### 路由表

用户直接修改此表添加/删除路由条目：

```c
static const struct {
    uint32_t            can_id;
    can_bus_handler_t   handler;
} s_can_bus_table[] = {
    /* 示例：用户按需填入 */
    /* { 0x100,  app_display_normal_can_handler }, */
    /* { 0x101,  app_display_normal_can_handler }, */
    /* { 0x7E0,  uds_feed_can_message            }, */
};
```

### 分发函数

```c
static void can_bus_dispatch(uint32_t id, const uint8_t *data, uint8_t len)
{
    for (size_t i = 0; i < sizeof(s_can_bus_table) / sizeof(s_can_bus_table[0]); i++)
    {
        if (s_can_bus_table[i].can_id == id)
        {
            s_can_bus_table[i].handler(id, data, len);
            return;
        }
    }
}
```

### 接收任务

```c
static void can_bus_task(void *arg)
{
    uint32_t id;
    uint8_t data[8];
    uint8_t len;

    while (1)
    {
        while (g_can_driver->receive(&id, data, &len))
        {
            can_bus_dispatch(id, data, len);
        }
        vTaskDelay(1);
    }
}
```

### 初始化

```c
void can_bus_init(void)
{
    g_can_driver = can_bus_port_get_driver();
    g_can_driver->init();
    g_can_driver->task_create(can_bus_task, NULL);

    /* isotp 共用同一个 CAN 驱动 */
    isotp_init_with_driver(g_can_driver);
}
```

### 发送

```c
int can_bus_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    return g_can_driver->send(id, data, len);
}
```

## `esp32_can_bus_port.c` — ESP32 端口实现

```c
#include "can_bus_port.h"
#include "can_bus.h"

#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ── 硬件配置宏（可通过编译选项覆盖）── */
#ifndef CAN_BUS_TWAI_TX_GPIO
#define CAN_BUS_TWAI_TX_GPIO   GPIO_NUM_21
#endif
#ifndef CAN_BUS_TWAI_RX_GPIO
#define CAN_BUS_TWAI_RX_GPIO   GPIO_NUM_22
#endif

static int esp32_can_bus_init(void)
{
    twai_general_config_t g_cfg = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = CAN_BUS_TWAI_TX_GPIO,
        .rx_io = CAN_BUS_TWAI_RX_GPIO,
        .clkout_io = GPIO_NUM_NC,
        .bus_off_io = GPIO_NUM_NC,
        .tx_queue_len = 16,
        .rx_queue_len = 16,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0,
    };
    twai_timing_config_t t_cfg = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK) return -1;
    if (twai_start() != ESP_OK) { twai_driver_uninstall(); return -2; }
    return 0;
}

static int esp32_can_bus_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    twai_message_t tx_msg;
    tx_msg.identifier       = id;
    tx_msg.data_length_code = (len > 8) ? 8 : len;
    tx_msg.extd             = 0;
    tx_msg.rtr              = 0;
    memset(tx_msg.data, 0, sizeof(tx_msg.data));
    if (data && len) memcpy(tx_msg.data, data, tx_msg.data_length_code);
    return (twai_transmit(&tx_msg, pdMS_TO_TICKS(100)) == ESP_OK) ? 0 : -1;
}

static int esp32_can_bus_receive(uint32_t *id, uint8_t *data, uint8_t *len)
{
    twai_message_t rx_msg;
    if (twai_receive(&rx_msg, 0) != ESP_OK) return 0;
    *id  = rx_msg.identifier;
    *len = rx_msg.data_length_code;
    memcpy(data, rx_msg.data, *len);
    return 1;
}

static int esp32_can_bus_task_create(void (*task)(void *), void *arg)
{
    return (xTaskCreate(task, "can_bus", 4096, arg, 5, NULL) == pdPASS) ? 0 : -1;
}

static uint32_t esp32_can_bus_get_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void esp32_can_bus_debug(const char *message, ...) { (void)message; }

const can_port_driver_t *can_bus_port_get_driver(void)
{
    static const can_port_driver_t driver = {
        .init        = esp32_can_bus_init,
        .send        = esp32_can_bus_send,
        .receive     = esp32_can_bus_receive,
        .task_create = esp32_can_bus_task_create,
        .get_ms      = esp32_can_bus_get_ms,
        .debug       = esp32_can_bus_debug,
    };
    return &driver;
}
```

## isotp 驱动接口统一

`isotp.h` 不再定义 `isotp_port_driver_t`，改为引用 `can_bus.h` 中的 `can_port_driver_t`：

```c
// isotp.h
#include "can_bus.h"

// 删除 isotp_port_driver_t 定义
// isotp_handle_t ... 不变
// isotp_init_with_driver() 参数类型改为 const can_port_driver_t *
```

`isotp.c` 中全局驱动指针类型变更：

```c
static const can_port_driver_t *g_isotp_driver;
```

`isotp_init_with_driver()` 参数类型同步变更。`g_isotp_driver->send`、`g_isotp_driver->get_ms`、
`g_isotp_driver->debug` 字段与 `can_port_driver_t` 完全同名，isotp 内部代码无需再改。

## 旧端口文件删除

| 文件 | 原因 |
|---|---|
| `can_diag/isotp/port/esp32_isotp_port.c` | 被 `esp32_can_bus_port.c` 取代 |
| `can_diag/isotp/port/stm32_isotp_port.c` | 同上 |
| `can_diag/isotp/port/isotp_port.h` | `can_bus_port_get_driver()` 取代 `isotp_port_get_driver()` |

## 依赖关系图

```
app_main()
    │
    can_bus_init()  ───────────────────────────────┐
    │                                               │
    ├── can_bus_port_get_driver()  (port)           │
    ├── g_can_driver->init()       (port, TWAI)     │
    ├── g_can_driver->task_create() (port, FreeRTOS) │
    └── isotp_init_with_driver(g_can_driver)  ──────┤
                                                     │
can_bus_task() (FreeRTOS 后台)                       │
    └── g_can_driver->receive()  (port, TWAI)       │
        └── can_bus_dispatch()   (core, 平台无关)    │
            └── s_can_bus_table[] ← 用户定义        │
                ├── uds_feed_can_message()  → isotp  │
                └── app_xxx_handler()        → 应用   │

main loop:
    uds_poll()
        └── isotp_poll()  (仅时序，不拉取帧)
```

## 用户使用流程

```c
void app_main(void)
{
    can_bus_init();   /* CAN 硬件 + 后台接收任务 + isotp 驱动注册 */
    uds_init();       /* 注册 isotp 句柄 + 回调 */

    while (1)
    {
        uds_poll();   /* ISO-TP 时序 + UDS 分发 */
        vTaskDelay(1);
    }
}
```

CAN 接收在后台 `can_bus_task` 中自动完成。用户只需维护 `s_can_bus_table[]` 路由表，
并在主循环中调用 `uds_poll()` 驱动诊断协议状态机。
