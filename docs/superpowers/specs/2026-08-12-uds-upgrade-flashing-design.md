# UDS 刷写升级协议 设计方案

日期: 2026-08-12
状态: 待评审

---

## 1. 背景与目标

现有 UDS 模块实现了诊断会话控制（0x10）、读/写 DID（0x22/0x2E）、安全访问（0x27）、
Tester Present（0x3E）五个 ISO 14229-1 诊断服务。现需补充完整的 ECU 刷写升级协议
（bootloader 模式），以便通过 CAN + ISO-TP 完成固件升级。

**目标**：在现有 `uds` 模块内扩增刷写相关服务，不新建独立模块，不破坏已有 5 个服务
的 API 和行为。

## 2. 刷写流程

以下为完整的 UDS 刷写流程（ISO 14229-1 + 行业惯例）：

```
① 0x10 Diagnostic Session Control  → Programming Session
② 0x28 Communication Control       → 静默常规通信（禁止 App/EOL 报文）
③ 0x85 Control DTC Setting         → 禁用 DTC（防止刷写触发故障码）
④ 0x27 Security Access             → 解锁安全等级 2
⑤ 0x31 Routine Control             → EraseMemory（擦除目标 Flash 区域）
⑥ 0x34 Request Download            → 声明起始地址 + 总大小
⑦ 0x36 Transfer Data               → 传输固件数据块（多块，每块带 BlockSequenceCounter）
⑧ 0x37 Request Transfer Exit       → 退出传输
⑨ 0x31 Routine Control             → CheckProgrammingDependencies（完整性校验）
⑩ 0x11 ECU Reset                   → HardReset（重启进入新固件）
```

## 3. 文件变更

所有变更在 `modules/can_bus/can_diag/uds/` 下：

| 文件 | 操作 | 说明 |
|---|---|---|
| `include/uds.h` | 修改 | `uds_config_t` 新增；`uds_init` 签名变为 `int uds_init(const uds_config_t *config)` |
| `core/uds_defs.h` | 修改 | 新增刷写 SID 枚举、`uds_flash_state_t`、补充 NRC 码 |
| `core/uds_service.h` | 修改 | 新增 7 个 handler 声明 |
| `core/uds_service.c` | 修改 | 新增 7 个 handler + `routine_table[]` + 刷写状态机逻辑 |
| `core/uds.c` | 修改 | `uds_init` 适配新 config；`uds_poll` 适配状态机 |
| `port/uds_port.h` | **新建** | port 层空函数声明（Flash 操作 + ECU 复位 + 通信控制 + DTC 控制） |

不新建子模块，直接在现有 `can_diag/uds/` 下扩增。
Port 层只放类型定义和空函数声明，具体实现由使用者在自己的项目中提供。

## 4. 新增服务

| SID | 服务名称 | 请求类型 | 简述 |
|---|---|---|---|
| 0x11 | ECU Reset | SID + Sub | 软复位 / 硬复位 |
| 0x28 | Communication Control | SID + Sub + Data | 开启/禁用指定类型通信 |
| 0x31 | Routine Control | SID + Sub + Data | 例程执行（擦除、校验等），基于 `routine_table[]` 分发 |
| 0x34 | Request Download | SID + Data | 声明下载地址、大小、格式 |
| 0x36 | Transfer Data | SID + Data | 传输固件数据块 |
| 0x37 | Request Transfer Exit | SID + Data | 退出传输，触发校验 |
| 0x85 | Control DTC Setting | SID + Sub | 开关 DTC 检测 |

## 5. 核心类型定义

### 5.1 uds_config_t（在 `uds.h` 中）

```c
typedef struct
{
    uint32_t request_can_id;
    uint32_t response_can_id;
} uds_config_t;

int uds_init(const uds_config_t *config);
```

向后兼容：传 `NULL` 则仅初始化诊断功能（无刷写），等价于旧版。

### 5.2 uds_flash_state_t（在 `uds_defs.h` 中）

```c
typedef enum
{
    UDS_FLASH_IDLE = 0,
    UDS_FLASH_DOWNLOAD_REQUESTED,
    UDS_FLASH_TRANSFERRING,
    UDS_FLASH_TRANSFER_COMPLETE,
    UDS_FLASH_VERIFIED,
} uds_flash_state_t;
```

### 5.3 uds_routine_t（在 `uds_service.h` 中）

```c
typedef struct
{
    uint16_t              routine_id;
    uds_session_t         session;
    uds_security_level_t  security;
    const char           *name;

    int (*start)(const uint8_t *req_data, uint16_t req_len);
} uds_routine_t;
```

`routine_table[]` 为 `static const` 编译期数组，空 handler 表示未实现。

### 5.4 服务类型扩展

`uds_defs.h` 中的 `uds_service_type_t` 新增 `UDS_SERVICE_TYPE_SID_DATA`（已有，0x34/0x36/0x37 共享）。

`services_table[]` 新增 7 个条目（在现有 64 槽中填充对应的 SID 位置）。

## 6. Port 层空函数声明

以下声明在 `port/uds_port.h` 中。使用者在自己的项目中提供实现。

```c
/* Flash 操作 */
uint32_t uds_port_flash_get_sector_size(uint32_t addr);
int      uds_port_flash_erase(uint32_t addr, uint32_t size);
int      uds_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
int      uds_port_flash_verify(uint32_t addr, uint32_t size, uint32_t *out_crc);

/* 系统操作 */
void uds_port_ecu_reset(void);
void uds_port_comm_control(uint8_t sub_func, uint8_t comm_type);
void uds_port_dtc_control(uint8_t on_off);
```

对齐策略：`uds_port_flash_write` 的实现负责内部 Read-Modify-Write 以适配 MCU 对齐要求，
协议层不做对齐处理。

所有 Flash 操作同步阻塞完成，不返回 0x78。

## 7. 刷写状态机

```
初始状态: UDS_FLASH_IDLE

0x34 RequestDownload:
  IDLE → DOWNLOAD_REQUESTED
  其他状态 → NRC 0x24 (RequestSequenceError)

0x36 TransferData:
  DOWNLOAD_REQUESTED / TRANSFERRING → TRANSFERRING
  其他状态 → NRC 0x24
  校验 blockSequenceCounter，不匹配 → NRC 0x24
  flash_write 失败 → NRC 0x26 (FailurePreventsExecution…)

0x37 RequestTransferExit:
  TRANSFERRING → TRANSFER_COMPLETE
  其他状态 → NRC 0x24

0x31 EraseMemory:
  任意状态 → IDLE (万能复位)

0x31 CheckProgDependencies:
  TRANSFER_COMPLETE → VERIFIED
  其他状态 → NRC 0x24

0x10 → Default Session:
  复位状态机到 IDLE

0x11 ECU Reset (HardReset):
  任意状态均可
```

## 8. 0x36 TransferData 流控

- `g_block_seq_expected` — 期望的下一个 BlockSequenceCounter（1..255）
- `g_flash_write_offset` — 当前写入偏移
- `g_flash_base_addr` — 由 0x34 设置
- `g_flash_total_size` — 由 0x34 设置

每块写入前：
1. 校验 `blockSeq == g_block_seq_expected`
2. 校验 `g_flash_write_offset + len <= g_flash_total_size`
3. 调用 `uds_port_flash_write(g_flash_base_addr + g_flash_write_offset, data, len)`
4. `g_flash_write_offset += len`; `g_block_seq_expected = (blockSeq + 1) & 0xFF`

## 9. 各服务核心逻辑

### 0x11 — ECU Reset
1. 校验 `sub_func` ∈ {0x01, 0x02, 0x03}
2. 组织正响应 `[0x51, sub_func]`
3. 若 `sub_func == 0x01` (HardReset)：同步调用 `uds_port_ecu_reset()`；其他类型发响应后返回，由应用层处理

### 0x28 — Communication Control
1. 校验 `sub_func` ∈ {0x00, 0x01, 0x03}
2. 校验 `comm_type` ∈ {0x01, 0x02, 0x03}
3. 发正响应 `[0x68, communicationType]`
4. 调用 `uds_port_comm_control(sub_func, comm_type)`

### 0x31 — Routine Control
1. `sub_func` = 0x01 (startRoutine)
2. 从 `routine_table[]` 查找 `routine_id`（2 字节大端）
3. 未找到 → NRC 0x31 (RequestOutOfRange)
4. `session` 和 `security` 门控不满足 → NRC 0x7F / 0x33
5. 调用 `routine->start(req_data, req_len)`
6. 返回 0 → 正响应；非 0 → NRC 0x26

### 0x34 — Request Download
1. 校验状态 == IDLE，否则 NRC 0x24
2. 解析 `dataFormatIdentifier`、`addressAndLengthFormatIdentifier`
3. 解析 `memoryAddress`、`memorySize`
4. 记录 `g_flash_base_addr`、`g_flash_total_size`、`g_flash_write_offset = 0`
5. 状态 → DOWNLOAD_REQUESTED
6. 组织正响应：`[0x74, lengthFormatIdentifier, maxNumberOfBlockLength (4字节)]`

### 0x35 — Request Upload（暂不实现）
预留 SID，`services_table[0x35] = {NULL}`。

### 0x36 — Transfer Data
1. 校验状态 ∈ {DOWNLOAD_REQUESTED, TRANSFERRING}
2. 提取 `blockSequenceCounter`（第 1 字节）
3. 校验 `blockSeq == g_block_seq_expected`
4. 校验 `g_flash_write_offset + len <= g_flash_total_size`
5. `uds_port_flash_write(base + offset, data, len)`
6. 更新 `g_block_seq_expected`、`g_flash_write_offset`
7. `g_block_seq_expected == 0` 时回绕为 1
8. 响应：`[0x76, blockSeq]`

### 0x37 — Request Transfer Exit
1. 校验状态 == TRANSFERRING
2. 可选：`uds_port_flash_verify(g_flash_base_addr, g_flash_write_offset, &crc)`
3. 状态 → TRANSFER_COMPLETE
4. 响应：`[0x77]` + 可选 CRC 值

### 0x85 — Control DTC Setting
1. 校验 `sub_func` ∈ {0x01, 0x02}
2. 响应 `[0xC5, sub_func]`
3. 调用 `uds_port_dtc_control(sub_func)`

## 10. 关键设计决策

| 决策 | 选择 | 理由 |
|---|---|---|
| Flash 抽象方式 | port 层声明空函数，使用者实现 | 固件中只有一份 Flash 实现，无需多态，避免 driver 注入样板代码 |
| 写入缓冲策略 | 逐块透传，收到即写 | 逻辑简单，对齐由 port 层 RMW 消化 |
| 耗时操作策略 | 同步阻塞，不返回 0x78 | 简单，适用于 bootloader 环境 |
| 例程扩展 | `routine_table[]` 静态数组，和 `services_table` 一致 | 风格统一，可扩展 |
| 状态机 | 枚举 `uds_flash_state_t`，违反序列即 NRC 0x24 | 协议合规 |
| 向后兼容 | `uds_init(NULL)` 等价旧版行为 | 不破坏诊断功能 |
| 操作外部模块的回调 | 空函数（非函数指针） | 编译期绑定，zero overhead |

## 11. 新增 NRC 码

```c
#define NRC_REQUEST_SEQUENCE_ERROR                      0x24 /* 已有，serivces_table 启动后使用 */
#define NRC_FAILURE_PREVENTS_EXECUTION                  0x26 /* 已有 */
#define NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED                0x70 /* 新增，请求含未支持的 compression/encrypt */
#define NRC_TRANSFER_DATA_SUSPENDED                     0x71 /* 预留 */
#define NRC_GENERAL_PROGRAMMING_FAILURE                 0x72 /* 新增 */
```

## 12. 不在本期范围内

- 0x38 Request File Transfer
- 0x35 Request Upload（从 ECU 读出 Flash 内容）
- 压缩/加密传输
- 断点续传
- 并行刷写多 ECU
