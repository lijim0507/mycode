# ISO-TP CAN 所有权分离设计

日期：2025-07-29
状态：已批准

## 概述

将 CAN 总线所有权从 isotp 模块移回应用层。变更后，isotp 成为纯协议状态机——不再拥有 CAN 硬件，不再主动拉取帧。应用层拥有 CAN 接收循环，将帧分发至各消费者（UDS/ISO-TP、odrive_can、应用自定义处理）。

## 问题

当前 isotp 模块通过 `isotp_port_driver_t` 拥有 CAN 硬件（`init`/`deinit` 管理 `twai_driver_install` 生命周期，`receive` 主动排空帧）。当系统运行普通 CAN 和 UDS 诊断流量时：

- isotp 的 `isotp_poll()` 通过 `driver->receive()` 排空**所有** CAN 帧
- 不匹配的帧被 `isotp_feed()` 静默丢弃
- 同一 TWAI 外设上不存在其他消费者——`odrive_can` 端口也会尝试安装其自己的 `twai_driver_install()`，将会失败

根本原因：isotp（一个协议层）承担了总线管理（一个基础设施层）的角色。

## 与上游对比

| 方面 | 上游 isotp-c | 当前代码 |
|---|---|---|
| CAN 所有权 | 用户拥有 CAN | isotp 通过端口驱动拥有 CAN |
| 帧馈送 | 用户调用 `isotp_on_can_message()` | `isotp_poll()` 内部调用 `isotp_feed()` |
| 过滤 | 用户需自行过滤 CAN ID | `isotp_feed()` 在排空后过滤 |
| 端口抽象 | 两个 shim 函数（send + get_ms） | 六函数指针结构体 |
| `isotp_poll()` | 纯时序（TX 节奏 + RX 超时） | 时序 + CAN 接收排空 |

## 设计决策

1. **保留所有既有函数名称。** `isotp_init()`、`isotp_feed()`、`isotp_poll()` 等均保留名称；改动仅限于内部实现和可见性。
2. **保留接收回调模式。** `isotp_register_recv_cb()` 与 `isotp_feed()` 仍然配合工作——回调在消息完整装配时触发。
3. **保留缓冲区注入模式。** `isotp_init_handle()` 继续接受外部分配的发送/接收缓冲区——零拷贝仍旧有效。
4. **无新增模块。** 不引入 `can_hub`——应用层自行实现分发循环。
5. **不消除单例。** `g_isotp_driver` 保持为模块级全局变量。多句柄支持不变。

## API 变更

### `isotp_port_driver_t`（`isotp.h`）

移除三个函数指针：

```c
// 移除前
typedef struct isotp_port_driver {
    int      (*init)(void);         // 已删除
    int      (*send)(...);          // 保留
    int      (*receive)(...);       // 已删除
    int      (*deinit)(void);       // 已删除
    uint32_t (*get_ms)(void);       // 保留
    void     (*debug)(...);         // 保留
} isotp_port_driver_t;

// 移除后
typedef struct isotp_port_driver {
    int      (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    uint32_t (*get_ms)(void);
    void     (*debug)(const char *message, ...);
} isotp_port_driver_t;
```

### `isotp_init()` / `isotp_init_with_driver()`（`isotp.c`）

语义变更：不再调用 `driver->init()`，仅验证并存储驱动指针。

```c
// 移除前
int isotp_init_with_driver(const isotp_port_driver_t *driver) {
    if (!driver || !driver->send || !driver->get_ms) return ISOTP_RET_ERROR;
    if (g_isotp_driver) isotp_deinit();
    if (driver->init && driver->init() != 0) return ISOTP_RET_ERROR;  // 已删除
    g_isotp_driver = driver;
    return ISOTP_RET_OK;
}

// 移除后
int isotp_init_with_driver(const isotp_port_driver_t *driver) {
    if (!driver || !driver->send || !driver->get_ms) return ISOTP_RET_ERROR;
    g_isotp_driver = driver;
    return ISOTP_RET_OK;
}
```

### `isotp_deinit()`（`isotp.c`）

语义变更：不再调用 `driver->deinit()`，仅置空指针。

```c
// 移除前
int isotp_deinit(void) {
    if (g_isotp_driver && g_isotp_driver->deinit) g_isotp_driver->deinit();  // 已删除
    g_isotp_driver = NULL;
    return ISOTP_RET_OK;
}

// 移除后
int isotp_deinit(void) {
    g_isotp_driver = NULL;
    return ISOTP_RET_OK;
}
```

### `isotp_feed()`（`isotp.c`）

从 `static` 变更为公开。名称、签名、内部逻辑不变。

```c
// 移除前
static void isotp_feed(isotp_handle_t *handle, uint32_t id, uint8_t *data, uint8_t len);

// 移除后
void isotp_feed(isotp_handle_t *handle, uint32_t id, uint8_t *data, uint8_t len);
```

声明添加至 `isotp.h`。

### `isotp_poll()`（`isotp.c`）

移除接收排空循环。所有其他逻辑不变。

```c
// 已删除（isotp.c，第 859-864 行）
if (g_isotp_driver->receive) {
    while (g_isotp_driver->receive(&id, data, &len)) {
        isotp_feed(handle, id, data, len);
    }
}
```

保留：接收超时处理（`receive_timer_cr`）、发送连续帧节奏控制（`send_timer_st`/`send_timer_bs`）。

### 以下函数不变

`isotp_init_handle()`、`isotp_register_recv_cb()`、`isotp_send()`、`isotp_send_with_id()`、`isotp_read()`——签名和实现均不变。

## 端口层变更

### `esp32_isotp_port.c`

| 元素 | 处置 |
|---|---|
| `esp32_isotp_init()` | 已删除 |
| `esp32_isotp_deinit()` | 已删除 |
| `esp32_isotp_receive()` | 已删除 |
| `esp32_isotp_twai_rx_callback()` | 已删除 |
| `g_rx_buf`（环形缓冲区 + 头/尾） | 已删除 |
| `esp32_isotp_send()` | 保留且不变 |
| `esp32_isotp_get_ms()` | 保留且不变 |
| `esp32_isotp_debug()` | 保留且不变 |
| `isotp_port_get_driver()` | 保留——返回更薄的结构体 |

### `stm32_isotp_port.c`

同上。

### `isotp_port.h`

不变——仅声明 `isotp_port_get_driver()`。

## UDS 层变更

### `uds_init()`（`uds.c`）

调用方式不变。`isotp_init()` 内部仍调用 `isotp_port_get_driver()`，只是 `isotp_init_with_driver()` 不再触发 `driver->init()`：

```c
// 不变——调用签名保持原样
int uds_init(void) {
    isotp_init();  // 内部执行 isotp_port_get_driver() → isotp_init_with_driver()，不初始化硬件
    isotp_init_handle(&g_isotp_handle, config->request_id, config->response_id, ...);
    isotp_register_recv_cb(&g_isotp_handle, uds_on_recv);
}
```

### `uds_deinit()`（`uds.c`）

清除已初始化标志，调用 `isotp_deinit()`。无变化。

### 新增：`uds_feed_can_message()`（`uds.c` + `uds.h`）

```c
void uds_feed_can_message(uint32_t id, const uint8_t *data, uint8_t len) {
    isotp_feed(&g_isotp_handle, id, data, len);
}
```

应用层在 CAN 接收循环中调用此函数，将诊断 CAN 帧路由至 UDS。

### 以下函数不变

`uds_poll()`、`uds_send_response()`、`uds_send_negative_response()`、`uds_send()`、`uds_read()`、`uds_get_session()`、`uds_set_session()`、`uds_get_security_level()`、`uds_set_security_level()`——签名和实现均不变。

## 应用层用法

```c
void app_main(void)
{
    // 1. 应用层初始化 CAN 硬件（只做一次）
    twai_driver_install(&twai_cfg, NULL);
    twai_start();

    // 2. 初始化诊断协议栈（不触碰 CAN 硬件）
    uds_init();

    while (1)
    {
        // 3. 应用层拥有 CAN 接收循环 —— 你来决定数据流向
        twai_message_t rx_msg;
        while (twai_receive(&rx_msg, 0) == ESP_OK)
        {
            if (rx_msg.identifier == 0x7E0)
            {
                uds_feed_can_message(rx_msg.identifier,
                                     rx_msg.data, rx_msg.data_length_code);
            }
            else if (rx_msg.identifier >= 0x020 && rx_msg.identifier <= 0x03F)
            {
                odrive_can_feed(rx_msg.identifier,
                                rx_msg.data, rx_msg.data_length_code);
            }
            else
            {
                app_can_handler(rx_msg.identifier,
                                rx_msg.data, rx_msg.data_length_code);
            }
        }

        // 4. 轮询协议状态机（仅时序，不拉取帧）
        uds_poll();
    }
}
```

## 影响面

| 文件 | 变更程度 |
|---|---|
| `modules/can_diag/isotp/include/isotp.h` | 中等 — 驱动结构体瘦身，`isotp_feed` 声明 |
| `modules/can_diag/isotp/core/isotp.c` | 中等 — init/deinit 简化，feed 公开化，poll 去接收排空 |
| `modules/can_diag/isotp/port/esp32_isotp_port.c` | 大幅精简 |
| `modules/can_diag/isotp/port/stm32_isotp_port.c` | 大幅精简 |
| `modules/can_diag/isotp/port/isotp_port.h` | 不变 |
| `modules/can_diag/uds/include/uds.h` | 轻微 — 新增 `uds_feed_can_message` |
| `modules/can_diag/uds/core/uds.c` | 轻微 — init 适配 |
| `modules/can_diag/uds/port/esp32_uds_port.c` | 可能不变 — 配置 ID 逻辑不变 |
