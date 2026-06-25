# MODULE_DESIGN

## 1. 设计定位

本项目的模块不是完整应用组件，而是可复制、可裁剪、可移植的嵌入式驱动代码片段。

模块设计优先满足：

- 硬件相关代码和核心逻辑分离。
- Core 层尽量可跨平台复用。
- Port 层集中处理 GPIO、SPI、I2C、CAN、UART、RMT、Tick 等平台差异。
- 上层模块通过抽象接口组合底层模块，而不是直接耦合具体硬件。
- 每个模块可以单独拿到实际项目中使用。

## 2. 标准三层模块结构

典型模块目录：

```text
module_name/
├── include/        对外 API、公共类型、模块配置
├── core/           核心逻辑、协议、算法、状态机
└── port/           平台适配接口和具体平台实现
```

### include 层

`include/xxx.h` 是模块对外入口，负责：

- 定义 public API。
- 定义对外可见类型。
- 定义用户需要配置或持有的 handle/config。
- 隐藏内部状态和平台细节。

示例：

- `eeprom/include/eeprom.h`
- `diag/isotp/include/isotp.h`
- `key/include/key.h`
- `ws281x/include/ws281x.h`

### core 层

`core/` 是模块主体，负责：

- 参数检查。
- 状态管理。
- 协议解析。
- 数据流控制。
- 状态机推进。
- 调用 port driver 或 transport 接口。

Core 层通常不直接使用平台外设 API。

### port 层

`port/` 负责硬件绑定，典型内容：

- `xxx_port.h`：声明 port driver 获取函数或 port 配置接口。
- `esp32_xxx_port.c`：ESP32 具体实现。
- `stm32_xxx_port.c`：STM32 具体实现。
- `linux_xxx_port.c`：Linux 模拟或测试实现。

Port 层内部处理引脚、外设句柄、时钟、队列、ISR、平台 SDK 调用。

## 3. Driver 注入模式

多数模块通过函数指针结构体抽象硬件能力。

典型模式：

```c
typedef struct
{
    int  (*init)(void);
    int  (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    int  (*receive)(uint32_t *id, uint8_t *data, uint8_t *len);
    int  (*deinit)(void);
} xxx_driver_t;
```

Core 初始化时注入 driver：

```c
int xxx_init(const xxx_driver_t *driver, const xxx_config_t *config);
```

Core 保存 driver 指针：

```c
static const xxx_driver_t *g_driver;
static bool g_initialized;
```

之后所有硬件访问都通过 `g_driver->xxx()` 完成。

适用模块：

- `swi2c`
- `imu`
- `ble`
- `foc`
- `isotp`

## 4. Port Driver Getter 模式

平台实现通过 getter 返回静态 driver 表。

典型形式：

```c
const xxx_driver_t *xxx_port_get_driver(void)
{
    static const xxx_driver_t driver = {
        .init   = esp32_xxx_init,
        .send   = esp32_xxx_send,
        .deinit = esp32_xxx_deinit,
    };

    return &driver;
}
```

上层使用：

```c
xxx_init(xxx_port_get_driver(), config);
```

这种方式让应用层只依赖模块 API，不需要知道具体平台函数名。

## 5. Port 自封装硬件配置

新 port 设计中，硬件参数应尽量放在 port 文件内部。

推荐：

```c
#ifndef ISOTP_ESP32_TWAI_TX_GPIO
#define ISOTP_ESP32_TWAI_TX_GPIO    GPIO_NUM_21
#endif

static int esp32_isotp_init(void)
{
    ...
}
```

目标：

- Core 层不接触引脚号、外设实例、平台句柄。
- 上层调用者不关心底层硬件细节。
- 用户需要修改硬件参数时，通过宏覆盖或编辑 port 文件完成。

历史模块中仍有 `void *port_cfg` 模式，新增模块优先使用 port 自封装方式。

## 6. Transport 抽象模式

部分模块不仅对接硬件，还会把自身 core API 再包装成更高层 transport，供其他模块复用。

典型例子是 `swi2c`：

```c
typedef struct i2c_transport
{
    int     (*start)(void);
    int     (*stop)(void);
    int     (*send_byte)(uint8_t byte);
    uint8_t (*receive_byte)(void);
    int     (*send_ack)(uint8_t bit);
    uint8_t (*receive_ack)(void);
} i2c_transport_t;
```

`swi2c_get_transport()` 将 `swi2c_start()`、`swi2c_stop()` 等 core API 组成 transport。

EEPROM 模块通过 `i2c_transport_t` 使用 I2C，不直接依赖 SWI2C 的内部实现。

这种模式适合：

- 一个底层模块被多个上层模块复用。
- 上层只需要协议传输能力，不需要知道底层是硬件 I2C 还是软件 I2C。
- 未来替换底层实现时保持上层 API 不变。

## 7. Adapter 适配模式

当 core 内部还集成芯片级子驱动时，可以用 adapter 把项目 driver 接口转换成芯片库需要的接口。

典型例子是 `imu`：

- `imu_driver_t` 由 port 层提供 SPI、CS、delay。
- `imu/core/imu.c` 内部持有 `icm42688_handle_t`。
- `imu_spi_send_adapter()`、`imu_spi_recv_adapter()`、`imu_set_cs_adapter()` 把 `imu_driver_t` 转成 `icm42688` 所需接口。

设计效果：

- `icm42688` 芯片逻辑可以保持独立。
- `imu` 模块对外提供统一 IMU API。
- 平台差异仍然只存在于 port 层。

适合用于：

- 传感器芯片驱动封装。
- 外部协议库接入。
- 将第三方/旧代码适配到本项目架构。

## 8. Handle 模式

部分模块使用 handle 表示一个逻辑实例。

典型例子：

- `isotp_handle_t`
- `ws281x_handle_t`
- `key_t`

### 调用方分配对象

`key_t` 属于调用方分配对象：

```c
key_t key;
key_init(&key);
```

特点：

- 支持多个实例。
- 内存生命周期由调用方控制。
- 适合轻量状态机。

### 模块返回不透明句柄

`ws281x_handle_t` 属于不透明句柄：

```c
typedef struct ws281x_dev *ws281x_handle_t;
```

特点：

- 隐藏内部结构。
- API 更稳定。
- 适合内部状态较复杂的模块。

### 协议状态句柄

`isotp_handle_t` 保存发送、接收、多帧状态、缓冲区、回调等协议状态。

特点：

- 协议逻辑可重入到不同 handle。
- 模块全局 driver 与实例状态分离。
- 适合 CAN ID、收发缓冲、状态机都与实例相关的协议。

## 9. 单例模块状态模式

部分模块采用模块级单例状态：

```c
static const xxx_driver_t *g_driver;
static bool g_initialized;
static xxx_config_t g_config;
```

适用场景：

- 底层硬件资源天然只有一个。
- 当前项目只需要一个实例。
- 模块复制到实际项目时希望简单直接。

典型模块：

- `eeprom`
- `swi2c`
- `imu`
- `ble`
- `uds`

单例模块应提供：

- `xxx_init()`
- `xxx_deinit()`
- `g_initialized` 检查
- 重复初始化时先释放旧资源或安全返回

## 10. 轮询状态机模式

多个模块使用非阻塞轮询推进状态机。

典型 API：

```c
void xxx_poll(void);
void xxx_process(void);
void xxx_scan(xxx_t *obj);
```

典型模块：

- `isotp_poll()`
- `uds_poll()`
- `uds_process()`
- `key_scan()`
- `key_manager_scan()`

设计原则：

- API 内部不长时间阻塞。
- 调用方在主循环、定时任务或 RTOS task 中周期调用。
- 时间判断通过 port 提供的 tick 或 driver `get_ms()` 完成。
- 状态机每次只推进一小步。

这种方式适合协议栈、按键检测、超时控制、多帧传输。

## 11. 回调事件模式

模块通过 callback 把异步事件交给上层。

典型例子：

```c
typedef void (*key_callback_t)(key_t *key, key_event_t event);
typedef void (*isotp_recv_cb_t)(uint8_t *data, uint16_t len);
```

使用方式：

- `key_register_callback()` 注册按键事件。
- `isotp_register_recv_cb()` 注册完整数据接收事件。
- UDS 使用 ISO-TP 接收回调置位请求标志，再由 `uds_process()` 处理。

设计原则：

- 回调中尽量少做重活。
- ISR 或接收回调只搬运数据、置标志。
- 复杂解析放到 `process()` 或 task 上下文执行。

## 12. 上层协议组合模式

上层协议模块通过组合底层模块构建更高层功能。

典型链路：

```text
UDS
└── ISO-TP
    └── CAN port driver
        └── ESP32 TWAI / STM32 CAN
```

UDS 模块不直接操作 CAN，而是：

1. 调用 `isotp_init()`。
2. 初始化 `isotp_handle_t`。
3. 注册 ISO-TP 接收回调。
4. 在 `uds_poll()` 中推进 ISO-TP。
5. 在 `uds_process()` 中解析 UDS SID 并调用 service handler。

另一个典型链路：

```text
EEPROM
└── i2c_transport_t
    └── SWI2C core
        └── SWI2C port
```

这种组合方式让上层模块依赖抽象能力，而不是依赖具体硬件。

## 13. Service Table 模式

协议类模块可使用服务表分发请求。

典型例子是 UDS：

```text
SID -> service type -> handler
```

处理流程：

1. 收到完整请求。
2. 读取 SID。
3. 查询服务表。
4. 按服务类型解析请求格式。
5. 调用 handler。
6. 生成正响应或负响应。

适合：

- UDS 诊断服务。
- AT 命令表。
- 自定义串口协议命令。
- BLE characteristic 命令分发。

## 14. 缓冲区与数据流设计

常见缓冲设计：

- 模块内部静态缓冲区：适合单例协议栈，如 UDS 请求缓冲。
- handle 内部缓冲指针：适合协议实例，如 ISO-TP。
- 调用方传入缓冲区：适合读写 API，如 EEPROM。
- ring buffer：适合 ISR 到 task 的数据交接，如 CAN RX。

数据流通常遵循：

```text
port 接收原始数据
-> core 收集 / 解码
-> 状态机判断完整性
-> callback 或 process 分发
-> 上层生成响应
-> core 编码
-> port 发送
```

## 15. 推荐的新模块模板

新增模块优先按以下方式设计：

```text
xxx/
├── include/
│   └── xxx.h
├── core/
│   └── xxx.c
└── port/
    ├── xxx_port.h
    └── esp32_xxx_port.c
```

`include/xxx.h`：

- public config / handle / enum
- public API
- 必要的 driver typedef

`core/xxx.c`：

- `static const xxx_driver_t *g_driver`
- `static bool g_initialized`
- 参数检查
- 状态机 / 协议 / 算法
- 不直接写平台外设代码

`port/xxx_port.h`：

```c
const xxx_driver_t *xxx_port_get_driver(void);
```

`port/esp32_xxx_port.c`：

- ESP-IDF include
- 硬件参数宏
- static platform context
- static platform functions
- static const driver table
- `xxx_port_get_driver()`

## 16. 选择哪种模块形态

| 场景 | 推荐形态 |
| --- | --- |
| 单硬件资源、简单驱动 | 单例模块 + driver 注入 |
| 多个逻辑实例 | handle 模式 |
| 上层复用底层总线能力 | transport 抽象 |
| 协议栈、多帧、超时 | poll 状态机 |
| 异步事件通知 | callback 模式 |
| ISR 收包、task 处理 | ring buffer + flag/process |
| 芯片子驱动封装 | adapter 模式 |
| 命令/服务分发 | service table 模式 |

## 17. 历史兼容原则

当前代码中已有一些历史设计差异：

- 旧模块可能仍使用 `void *port_cfg`。
- 个别 core 会直接包含平台相关头文件。
- 某些模块还不是完整三层结构。
- CMake 中的模块列表与实际目录可能不完全一致。

处理原则：

1. 新模块优先遵循本文档。
2. 修改旧模块时先保持原有 API 可用。
3. 不在无关任务中大规模迁移架构。
4. 只有用户明确要求时，才做模块结构升级或接口破坏性调整。
