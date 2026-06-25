# CODING_STYLE

## 1. 总体原则

本项目新增代码统一使用 C 语言常见的 `snake_case` 风格。

命名应优先表达所属模块、对象含义和动作含义，避免过短、过泛、难以迁移的名称。

推荐命名顺序：

```text
模块前缀 + 对象/功能 + 动作/属性
```

示例：

```c
eeprom_read_bytes()
ws281x_set_pixel()
isotp_port_get_driver()
key_filter_process()
```

## 2. 文件注释框架

新增 `.c` / `.h` 文件使用统一 section 注释框架。

`.c` 文件推荐顺序：

```c
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
```

`.h` 文件推荐顺序：

```c
#ifndef __XXX_H_
#define __XXX_H_

/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Variables                              */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
```

头文件或源文件没有对应内容时，可以保留空 section，也可以按当前文件已有风格省略。

编辑旧文件时优先保持局部一致，不为了修正 section 拼写或对齐问题而全文件重排。

## 3. 文件命名

文件名使用小写字母 + 下划线。

| 文件类型 | 命名规则 | 示例 |
| --- | --- | --- |
| public header | `模块名.h` | `eeprom.h` |
| core source | `模块名.c` | `eeprom.c` |
| core 子功能 | `模块名_功能.c` | `ws281x_effect.c` |
| port header | `模块名_port.h` | `isotp_port.h` |
| platform port | `平台_模块名_port.c` | `esp32_isotp_port.c` |

模块目录、文件名、函数前缀尽量保持一致。历史模块已有命名不统一时，新增文件优先跟随该模块当前主命名。

## 4. 函数命名

函数使用 `snake_case`，并带模块前缀。

格式：

```text
module_action_object()
module_object_action()
```

两种格式都可用，但同一模块内应保持一致。

示例：

```c
eeprom_init()
eeprom_deinit()
eeprom_read_bytes()
eeprom_write_bytes()

ws281x_set_pixel()
ws281x_set_all()
ws281x_show_async()

key_init()
key_scan()
key_manager_scan()
```

平台 port 内部 static 函数应带平台前缀：

```c
esp32_isotp_init()
esp32_isotp_send()
stm32_udisk_mount()
linux_key_read()
```

回调函数建议使用 `_callback` 或 `_cb` 后缀，同一文件内保持统一：

```c
ble_access_callback()
ble_register_cb()
esp32_isotp_twai_rx_callback()
```

获取 port driver 的函数统一使用：

```c
xxx_port_get_driver()
```

如果返回的不是完整 driver，而是某类 transport，可使用：

```c
xxx_port_get_i2c()
xxx_port_get_spi()
```

## 5. 变量命名

变量使用 `snake_case`。

示例：

```c
uint16_t remaining;
uint16_t offset;
uint16_t chunk;
uint8_t  g_initialized;
```

局部变量应简洁但不能失去含义：

```c
ret
len
id
index
address
timeout_ms
```

不推荐新增无意义缩写：

```c
tmp1
data2
foo
bar
```

## 6. 全局与静态变量命名

模块内部静态全局变量使用 `g_` 前缀。

示例：

```c
static bool g_initialized;
static eeprom_config_t g_config;
static const ble_driver_t *g_driver;
```

静态上下文对象建议使用：

```c
g_ctx
g_xxx_ctx
```

示例：

```c
static esp32_eeprom_spi_ctx_t g_spi_ctx;
```

除非确实需要对外暴露，不新增非 `static` 全局变量。

## 7. 类型命名

所有新增 `struct`、`enum`、函数指针类型都应 typedef，并使用 `_t` 后缀。

结构体类型：

```c
typedef struct
{
    uint32_t id;
    uint8_t  data[8];
    uint8_t  dlc;
} isotp_frame_t;
```

枚举类型：

```c
typedef enum
{
    EEPROM_TYPE_I2C = 0,
    EEPROM_TYPE_SPI,
} eeprom_type_t;
```

不透明句柄类型：

```c
typedef struct ws281x_dev *ws281x_handle_t;
```

前向声明类型：

```c
typedef struct i2c_transport i2c_transport_t;
```

## 8. 结构体成员命名

新增结构体成员使用 `snake_case`。

示例：

```c
typedef struct
{
    uint8_t  device_addr;
    uint16_t page_size;
    uint32_t memory_size;
    uint8_t  addr_bytes;
} eeprom_config_t;
```

函数指针成员使用动作名或动作 + 对象名：

```c
typedef struct
{
    int  (*init)(void);
    int  (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    int  (*receive)(uint32_t *id, uint8_t *data, uint8_t *len);
    int  (*deinit)(void);
} isotp_port_driver_t;
```

历史代码中的 camelCase 成员可以保留，新增代码不继续扩散。

## 9. 枚举值命名

枚举值使用全大写下划线，并带模块前缀。

格式：

```text
MODULE_CATEGORY_VALUE
```

示例：

```c
EEPROM_TYPE_I2C
EEPROM_TYPE_SPI

WS281X_COLOR_RED
WS281X_COLOR_GREEN
WS281X_COLOR_NUM

ISOTP_SEND_STATUS_IDLE
ISOTP_SEND_STATUS_WAIT_FC
```

状态机枚举建议包含状态类别：

```c
KEY_ST_IDLE
KEY_ST_PRESSED
KEY_ST_WAIT_RELEASE
```

数量或结束标记可使用 `_NUM`、`_MAX`、`_END`，同一枚举内保持一致。

## 10. 宏命名

宏使用全大写下划线。

模块私有宏应带模块前缀：

```c
EEPROM_WRITE_CYCLE_MS
EEPROM_SPI_WRITE_TIMEOUT_MS
ISOTP_ESP32_RX_BUF_SIZE
```

平台配置宏建议包含模块名、平台名和硬件含义：

```c
ISOTP_ESP32_TWAI_TX_GPIO
ISOTP_ESP32_TWAI_RX_GPIO
```

可被外部编译选项覆盖的宏使用：

```c
#ifndef ISOTP_ESP32_RX_BUF_SIZE
#define ISOTP_ESP32_RX_BUF_SIZE     16
#endif
```

布尔类宏可使用：

```c
XXX_ENABLE_YYY
XXX_DISABLE_YYY
XXX_HAS_YYY
```

## 11. 常量命名

`static const` 常量使用 `snake_case` 或模块内既有风格。

如果常量用于预处理、数组大小、条件编译，优先使用宏。

如果常量需要类型检查，优先使用 `static const`：

```c
static const uint8_t g_default_addr = 0x50;
```

## 12. 参数命名

函数参数使用 `snake_case`，名称应体现方向和含义。

常用参数名：

```c
config
driver
handle
address
data
size
len
timeout_ms
channel
index
```

输出参数可以使用明确名称，不强制 `out_` 前缀：

```c
uint8_t *data
uint8_t *len
float *alpha
float *beta
```

当输入和输出容易混淆时，可以使用 `out_`：

```c
uint8_t *out_data
uint16_t *out_len
```

## 13. 返回值命名

局部返回值变量统一使用：

```c
ret
rc
```

同一函数内不要混用太多返回变量名。

推荐：

```c
int ret;

ret = xxx_do_something();
if (ret != 0)
{
    return ret;
}
```

ESP-IDF 返回值可使用 `err` 或 `ret`，同一文件内保持一致。

## 14. 缩写规则

常见嵌入式缩写可以直接使用：

```text
adc, dac, gpio, i2c, spi, uart, can, ble, imu, isotp, uds, pwm, dma, rmt
```

缩写在函数和变量中保持小写：

```c
esp32_spi_transfer()
eeprom_i2c_read_chunk()
ws281x_hsv2rgb()
```

宏和枚举值中保持大写：

```c
EEPROM_TYPE_I2C
ISOTP_ESP32_TWAI_TX_GPIO
```

不要新增项目外难以理解的个人缩写。

## 15. 动词建议

常用动作命名保持一致：

| 动作 | 用途 |
| --- | --- |
| `init` | 初始化 |
| `deinit` | 反初始化 |
| `start` / `stop` | 启动 / 停止 |
| `read` / `write` | 读 / 写 |
| `send` / `receive` | 发送 / 接收 |
| `set` / `get` | 设置 / 获取 |
| `enable` / `disable` | 使能 / 关闭 |
| `open` / `close` | 打开 / 关闭资源 |
| `process` | 处理一步逻辑 |
| `parse` | 解析输入数据 |
| `poll` | 轮询 |
| `scan` | 扫描 |
| `clear` | 清空数据或状态 |
| `reset` | 复位状态 |

避免同一模块中混用近义动词，例如同时大量使用 `fetch`、`load`、`get` 表达同一件事。

## 16. 历史命名处理

仓库中已有部分历史不一致命名，例如：

- `ws281x` 与旧 `ws2812` 引用
- 少量 camelCase 成员或局部变量
- 部分 section 拼写遗留
- 部分接口仍带旧式 `void *config`

处理原则：

1. 新增代码遵守本文档。
2. 修改旧代码时，优先保持模块局部一致。
3. 不因命名规范做全仓库无关重命名。
4. 需要破坏 API 的命名调整时，应由用户明确要求。
