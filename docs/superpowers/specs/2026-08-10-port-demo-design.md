# Port Demo — 统一底层驱动接口规范

**日期**: 2026-08-10
**状态**: 已评审

---

## 1. 定位

`modules/port demo/` 是项目中所有底层总线的**纯类型定义目录**。它只包含抽象接口的 typedef + getter 声明，**不包含任何实现代码**。

所有后续新增模块引用这里的接口，不再各自定义私有的 port driver。

---

## 2. 四条总线接口

### 2.1 SPI — `spi_port_driver_t`

```c
typedef struct
{
    int  (*init)(void);
    int  (*deinit)(void);
    void (*cs_set)(uint8_t level);
    int  (*spi_write)(const uint8_t *data, uint16_t len);
    int  (*spi_read)(uint8_t *data, uint16_t len);
    int  (*spi_transceive)(const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*delay_us)(uint32_t us);
    void (*delay_ms)(uint32_t ms);
} spi_port_driver_t;
```

### 2.2 I2C — `i2c_port_driver_t`

```c
typedef struct
{
    int  (*init)(void);
    int  (*deinit)(void);
    int  (*write)(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
    int  (*read)(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);
    int  (*write_read)(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                       uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);
} i2c_port_driver_t;
```

### 2.3 CAN — `can_port_driver_t`

```c
typedef struct
{
    int  (*init)(void);
    int  (*deinit)(void);
    int  (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    int  (*receive)(uint32_t *id, uint8_t *data, uint8_t *len, uint32_t timeout_ms);
} can_port_driver_t;
```

### 2.4 UART — `uart_port_driver_t`

```c
typedef struct
{
    int  (*init)(void);
    int  (*deinit)(void);
    int  (*send)(const uint8_t *data, uint32_t len);
    int  (*receive)(uint8_t *data, uint32_t len, uint32_t timeout_ms);
} uart_port_driver_t;
```

---

## 3. 统一规则

| 规则 | 内容 |
|---|---|
| init 签名 | 全部 `int (*)(void)`，不携带配置参数 |
| getter 命名 | `spi/i2c/can/uart_port_get_driver()`，返回 `const xxx_port_driver_t *` |
| 命名风格 | `snake_case` 标识符，`UPPER_CASE` 宏，`_t` typedef 后缀 |
| 接收方式 | 全部同步 poll 原语，receive 带 `timeout_ms`。ISR/回调/RingBuffer 在 port 实现内部处理 |
| 文件结构 | 每个文件 = header guard + typedef + getter 声明，"Includes" 下只放 `<stdint.h>` |

---

## 4. 目录结构

```
modules/port demo/
├── spi_port.h    →  spi_port_driver_t
├── i2c_port.h    →  i2c_port_driver_t
├── can_port.h    →  can_port_driver_t
└── uart_port.h   →  uart_port_driver_t
```

---

## 5. I2C 分层说明

`i2c_port_driver_t` 定义的是 **传输层**（带设备地址的事务级读写）。软件 I2C（GPIO 位带操作层）是 SWI2C 模块的内部实现细节，不出现在 port demo 中。

SWI2C 模块通过其 port 实现 `swi2c_driver_t` → core 将其包装为 `i2c_transport_t` → 上层模块通过 `i2c_port_get_driver()` 获取。
