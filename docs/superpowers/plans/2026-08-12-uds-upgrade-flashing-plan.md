# UDS 刷写升级协议 实现计划

> **给执行者:** 使用 superpowers:subagent-driven-development 或 superpowers:executing-plans 按任务顺序实现。
> 步骤使用 checkbox (`- [ ]`) 语法跟踪进度。

**目标:** 在现有 `uds` 模块内扩增 7 个刷写相关 UDS 服务 (0x11/0x28/0x31/0x34/0x36/0x37/0x85)，不破坏已有诊断服务。

**架构:** 类型定义层 (uds_defs.h, uds.h, uds_service.h) → port 空函数声明层 (uds_port.h) → 核心引擎层 (uds.c) → 服务实现层 (uds_service.c)。刷写状态机在 uds_service.c 中管理，通过 uds_set_session() 钩子复位。

**涉及文件:** `uds_port.h` 新建，其余 5 个文件修改。

## 全局约束

- 代码标识符使用英文 `snake_case`，注释和文档使用中文
- Braces 使用 Allman 风格（单独一行）
- Header guards 使用 `#ifndef __FILENAME_H_`
- 不破坏现有 5 个服务的 API 和行为
- `uds_init(NULL)` 保持向后兼容（纯诊断模式）
- 所有 Flash 操作同步阻塞完成

---

### Task 1: 新建 `port/uds_port.h` — Port 层空函数声明

**文件:**
- 创建: `modules/can_bus/can_diag/uds/port/uds_port.h`

**接口:**
- 消费: 无
- 产出: 7 个 `extern` 空函数声明，由使用者在自己的项目中实现

- [ ] **Step 1: 创建 port 目录并写入文件**

```bash
mkdir -p modules/can_bus/can_diag/uds/port
```

```c
#ifndef __UDS_PORT_H_
#define __UDS_PORT_H_

/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>

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
/*                      Exported Functions                                  */
/****************************************************************************/

/**
 * @brief 查询指定地址的扇区大小（对齐粒度）
 *
 * @param addr Flash 地址
 * @return uint32_t 扇区大小（字节）
 */
uint32_t uds_port_flash_get_sector_size(uint32_t addr);

/**
 * @brief 擦除指定范围的 Flash（同步阻塞直到完成）
 *
 * @param addr  起始地址
 * @param size  擦除大小（字节）
 * @return int  0 成功，负值失败
 */
int      uds_port_flash_erase(uint32_t addr, uint32_t size);

/**
 * @brief 写入数据到 Flash（实现层负责内部 RMW 对齐）
 *
 * @param addr  起始地址
 * @param data  数据指针
 * @param len   数据长度（字节）
 * @return int  0 成功，负值失败
 */
int      uds_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);

/**
 * @brief 校验已写入数据的 CRC32
 *
 * @param addr     起始地址
 * @param size     校验范围（字节）
 * @param out_crc  输出 CRC32 值
 * @return int     0 校验通过，负值失败
 */
int      uds_port_flash_verify(uint32_t addr, uint32_t size, uint32_t *out_crc);

/**
 * @brief ECU 复位（HardReset）
 */
void uds_port_ecu_reset(void);

/**
 * @brief 通信控制（启用/禁用指定类型的通信）
 *
 * @param sub_func  控制类型 0x00=EnableAll 0x01=EnableRxDisableTx 0x03=DisableAll
 * @param comm_type 通信类型 0x01=Normal 0x02=NetworkMgmt 0x03=Both
 */
void uds_port_comm_control(uint8_t sub_func, uint8_t comm_type);

/**
 * @brief DTC 设置控制
 *
 * @param on_off 0x01=On 0x02=Off
 */
void uds_port_dtc_control(uint8_t on_off);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
```

- [ ] **Step 2: 提交**

```bash
git add modules/can_bus/can_diag/uds/port/uds_port.h
git commit -m "feat(uds): 新建 port/uds_port.h，声明刷写相关空函数"
```

---

### Task 2: 更新 `core/uds_defs.h` — 新增枚举和 NRC 码

**文件:**
- 修改: `modules/can_bus/can_diag/uds/core/uds_defs.h`

**接口:**
- 消费: 无
- 产出: 7 个新 SID 枚举名、`uds_flash_state_t`、3 个新 NRC 宏

- [ ] **Step 1: 在 enum 中将 RESERVED 替换为命名的 SID**

找到 `uds_service_id_t` 枚举，替换以下行：

```c
    UDS_SERVICE_ID_RESERVED_11 = 0x11,
```
→
```c
    UDS_SERVICE_ID_ECU_RESET = 0x11,
```

```c
    UDS_SERVICE_ID_RESERVED_28 = 0x28,
```
→
```c
    UDS_SERVICE_ID_COMMUNICATION_CONTROL = 0x28,
```

```c
    UDS_SERVICE_ID_RESERVED_31 = 0x31,
```
→
```c
    UDS_SERVICE_ID_ROUTINE_CONTROL = 0x31,
```

```c
    UDS_SERVICE_ID_RESERVED_34 = 0x34,
```
→
```c
    UDS_SERVICE_ID_REQUEST_DOWNLOAD = 0x34,
```

```c
    UDS_SERVICE_ID_RESERVED_36 = 0x36,
```
→
```c
    UDS_SERVICE_ID_TRANSFER_DATA = 0x36,
```

```c
    UDS_SERVICE_ID_RESERVED_37 = 0x37,
```
→
```c
    UDS_SERVICE_ID_REQUEST_TRANSFER_EXIT = 0x37,
```

```c
    UDS_SERVICE_ID_RESERVED_85 = 0x85,   /* 注意：此枚举值需要新增，不在现有 0x00-0x40 范围内 */
```

实际上 `UDS_SERVICE_ID_RESERVED_MAX = 0x40`，0x85 超出范围。需要把 `UDS_SERVICE_TABLE_SIZE` 从 0x40 扩展到 0x86（或单独处理 0x85）。

**方案**：0x11/0x28/0x31/0x34/0x36/0x37 在现有 64 槽范围内直接替换 RESERVED；0x85 (0x85 = 133) 需要扩展表格大小。

修改 `UDS_SERVICE_TABLE_SIZE`：
```c
#define UDS_SERVICE_TABLE_SIZE      0x86  /* 0x00..0x85 = 134 */
```

并在枚举中新增 0x40..0x84 的 RESERVED 条目和 0x85：

枚举末尾（`UDS_SERVICE_ID_RESERVED_3F = 0x3F,` 之后、`UDS_SERVICE_ID_RESERVED_MAX` 之前）插入从 0x40 到 0x85 的条目。为了简洁，用 `/* 0x40..0x84 reserved */` 注释代替逐个定义，只显式定义 0x85：

```c
    UDS_SERVICE_ID_RESERVED_3F = 0x3F,
    /* 0x40..0x84 reserved for future use */
    UDS_SERVICE_ID_CONTROL_DTC_SETTING = 0x85,
    UDS_SERVICE_ID_RESERVED_MAX = 0x86,
```

- [ ] **Step 2: 新增 `uds_flash_state_t` 枚举**

在 `uds_handler_result_t` 后面新增：

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

- [ ] **Step 3: 新增 NRC 码**

在 NRC 定义区域末尾添加：

```c
#define NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED                0x70
#define NRC_TRANSFER_DATA_SUSPENDED                     0x71
#define NRC_GENERAL_PROGRAMMING_FAILURE                 0x72
```

- [ ] **Step 4: 提交**

```bash
git add modules/can_bus/can_diag/uds/core/uds_defs.h
git commit -m "feat(uds): uds_defs.h 新增刷写 SID 枚举、flash_state_t、NRC 码"
```

---

### Task 3: 更新 `include/uds.h` — uds_config_t 和 uds_init 签名

**文件:**
- 修改: `modules/can_bus/can_diag/uds/include/uds.h`

**接口:**
- 消费: 无
- 产出: `uds_config_t` 类型、`int uds_init(const uds_config_t *config)` 声明

- [ ] **Step 1: 新增 `uds_config_t` 结构体**

在 `uds_port_config_t` 后新增：

```c
typedef struct
{
    uint32_t request_can_id;
    uint32_t response_can_id;
} uds_config_t;
```

- [ ] **Step 2: 修改 `uds_init` 声明**

将：
```c
int  uds_init(void);
```
改为：
```c
int  uds_init(const uds_config_t *config);
```

- [ ] **Step 3: 新增 flash 状态复位函数声明**

在 `uds_set_security_level` 后新增：

```c
/**
 * @brief 复位刷写状态机到 IDLE
 */
void uds_flash_reset_state(void);
```

- [ ] **Step 4: 提交**

```bash
git add modules/can_bus/can_diag/uds/include/uds.h
git commit -m "feat(uds): uds.h 新增 uds_config_t，修改 uds_init 签名"
```

---

### Task 4: 更新 `core/uds_service.h` — 新增类型和 handler 声明

**文件:**
- 修改: `modules/can_bus/can_diag/uds/core/uds_service.h`

**接口:**
- 消费: `uds.h` 中的 `uds_session_t`、`uds_security_level_t`；`uds_defs.h` 中的 `uds_handler_result_t`、`uds_request_t`、`uds_response_t`
- 产出: `uds_routine_t` 类型、7 个 handler 声明、`routine_table` + `routine_table_size` extern

- [ ] **Step 1: 新增 `uds_routine_t` 结构体**

在 `uds_data_identifier_t` 后新增：

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

- [ ] **Step 2: 新增 7 个 handler 声明**

在现有 handler 声明后新增：

```c
uds_handler_result_t uds_ecu_reset(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_communication_control(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_routine_control(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_request_download(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_transfer_data(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_request_transfer_exit(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_control_dtc_setting(uds_request_t *req, uds_response_t *resp);
```

- [ ] **Step 3: 新增 routine_table 外部声明**

```c
extern const uds_routine_t routine_table[];
extern const uint8_t       routine_table_size;
```

- [ ] **Step 4: 提交**

```bash
git add modules/can_bus/can_diag/uds/core/uds_service.h
git commit -m "feat(uds): uds_service.h 新增 uds_routine_t 和 7 个 handler 声明"
```

---

### Task 5: 更新 `core/uds_service.c` — 服务表 + handler 实现 + routine 表 + 状态机

**文件:**
- 修改: `modules/can_bus/can_diag/uds/core/uds_service.c`

**接口:**
- 消费: `uds_port.h` 中的 7 个空函数；Task 2-4 中定义的所有类型
- 产出: `routine_table[]`、`routine_table_size`、`services_table[]` 中的 7 个新条目、`uds_flash_reset_state()`

- [ ] **Step 1: 添加 `#include "uds_port.h"`**

在现有 includes 后添加：

```c
#include "uds_port.h"
```

- [ ] **Step 2: 新增刷写状态机全局变量**

在现有 `data_table[]` 定义之后、`services_table[]` 之前插入：

```c
/* ===== 刷写状态机 ===== */
static uds_flash_state_t g_flash_state         = UDS_FLASH_IDLE;
static uint32_t          g_flash_base_addr     = 0;
static uint32_t          g_flash_total_size    = 0;
static uint32_t          g_flash_write_offset  = 0;
static uint8_t           g_block_seq_expected  = 1;
```

- [ ] **Step 3: 新增 routine_table[] 和 routine_table_size**

在刷写状态机变量之后、`services_table[]` 之前插入：

```c
#define UDS_ROUTINE_TABLE_MAX  2

const uds_routine_t routine_table[UDS_ROUTINE_TABLE_MAX] = {
    {0xFF00, UDS_SESSION_PROGRAMMING, UDS_SEC_LEVEL_2, "EraseMemory",
     uds_port_flash_erase_start},
    {0xFF01, UDS_SESSION_PROGRAMMING, UDS_SEC_LEVEL_2, "CheckProgrammingDependencies",
     uds_port_flash_verify_start},
};

const uint8_t routine_table_size = UDS_ROUTINE_TABLE_MAX;
```

等一下，`uds_port_flash_erase_start` 和 `uds_port_flash_verify_start` 不存在——我们定义的是 `uds_port_flash_erase` 和 `uds_port_flash_verify`。routine 的 `start` 回调签名是 `int (*start)(const uint8_t *req_data, uint16_t req_len)`，但 Flash 函数签名不同。

需要两个适配函数来桥接：

```c
/* 桥接函数：将 routine 回调签名适配到 port 空函数 */
static int uds_routine_erase_memory(const uint8_t *req_data, uint16_t req_len)
{
    uint32_t addr;
    uint32_t size;

    if (req_len < 8)
    {
        return -1;
    }

    addr = ((uint32_t)req_data[0] << 24) | ((uint32_t)req_data[1] << 16)
         | ((uint32_t)req_data[2] << 8)  |  (uint32_t)req_data[3];
    size = ((uint32_t)req_data[4] << 24) | ((uint32_t)req_data[5] << 16)
         | ((uint32_t)req_data[6] << 8)  |  (uint32_t)req_data[7];

    return uds_port_flash_erase(addr, size);
}

static int uds_routine_check_dependencies(const uint8_t *req_data, uint16_t req_len)
{
    (void)req_data;
    (void)req_len;

    /* 校验已写入区域 */
    return uds_port_flash_verify(g_flash_base_addr, g_flash_write_offset, NULL);
}
```

然后 routine_table 使用这两个 static 函数：

```c
#define UDS_ROUTINE_TABLE_MAX  2

static int uds_routine_erase_memory(const uint8_t *req_data, uint16_t req_len);
static int uds_routine_check_dependencies(const uint8_t *req_data, uint16_t req_len);

const uds_routine_t routine_table[UDS_ROUTINE_TABLE_MAX] = {
    {0xFF00, UDS_SESSION_PROGRAMMING, UDS_SEC_LEVEL_2, "EraseMemory",
     uds_routine_erase_memory},
    {0xFF01, UDS_SESSION_TRANSFER_COMPLETE, UDS_SEC_LEVEL_2, "CheckProgrammingDependencies",
     uds_routine_check_dependencies},
};

const uint8_t routine_table_size = UDS_ROUTINE_TABLE_MAX;
```

等等，`UDS_SESSION_TRANSFER_COMPLETE` 不存在——那不是 session 类型。CheckProgDependencies 应该在 Programming Session 下执行。按 spec，它的 session 要求是 `UDS_SESSION_PROGRAMMING`。修正：

```c
    {0xFF01, UDS_SESSION_PROGRAMMING, UDS_SEC_LEVEL_2, "CheckProgrammingDependencies",
     uds_routine_check_dependencies},
```

- [ ] **Step 4: 更新 `services_table[]`**

将 `services_table[]` 中 7 个 RESERVED 条目替换为：

```c
    /* [0x11] */ {UDS_SERVICE_ID_ECU_RESET,              UDS_SERVICE_TYPE_SID_SUB,     uds_ecu_reset},
    /* [0x28] */ {UDS_SERVICE_ID_COMMUNICATION_CONTROL,  UDS_SERVICE_TYPE_SID_SUB_DATA, uds_communication_control},
    /* [0x31] */ {UDS_SERVICE_ID_ROUTINE_CONTROL,        UDS_SERVICE_TYPE_SID_SUB_DATA, uds_routine_control},
    /* [0x34] */ {UDS_SERVICE_ID_REQUEST_DOWNLOAD,       UDS_SERVICE_TYPE_SID_DATA,     uds_request_download},
    /* [0x36] */ {UDS_SERVICE_ID_TRANSFER_DATA,          UDS_SERVICE_TYPE_SID_DATA,     uds_transfer_data},
    /* [0x37] */ {UDS_SERVICE_ID_REQUEST_TRANSFER_EXIT,  UDS_SERVICE_TYPE_SID_DATA,     uds_request_transfer_exit},
    /* [0x85] */ {UDS_SERVICE_ID_CONTROL_DTC_SETTING,    UDS_SERVICE_TYPE_SID_SUB,     uds_control_dtc_setting},
```

注意：`services_table` 数组大小从 `UDS_SERVICE_ID_RESERVED_MAX`（旧值 0x40）变为 0x86。需要同步更新 `services_table` 的定义行：

```c
const uds_service_t services_table[UDS_SERVICE_TABLE_SIZE] = {
```

并且补全中间 0x40..0x84（共 69 个槽）的 RESERVED 条目。太长了——改为在 init 时动态初始化或使用注释标记。实际上 C 语言的 `static const` 数组如果只初始化部分元素，剩余自动填 0，恰好 `{0, UDS_SERVICE_TYPE_NONE, NULL}` 就是"未实现"的意思。

但现有代码是显式逐个列出的。为了最小化改动，采用**命名初始化**：

```c
const uds_service_t services_table[UDS_SERVICE_TABLE_SIZE] = {
    [0x10] = {UDS_SERVICE_ID_DIAGNOSTIC_SESSION_CONTROL, UDS_SERVICE_TYPE_SID_SUB,     uds_diag_session_control},
    [0x11] = {UDS_SERVICE_ID_ECU_RESET,                  UDS_SERVICE_TYPE_SID_SUB,     uds_ecu_reset},
    [0x22] = {UDS_SERVICE_ID_READ_DATA_BY_IDENTIFIER,    UDS_SERVICE_TYPE_SID_DATA,    uds_read_data_by_id},
    [0x27] = {UDS_SERVICE_ID_SECURITY_ACCESS,            UDS_SERVICE_TYPE_SID_SUB_DATA, uds_security_access},
    [0x28] = {UDS_SERVICE_ID_COMMUNICATION_CONTROL,      UDS_SERVICE_TYPE_SID_SUB_DATA, uds_communication_control},
    [0x2E] = {UDS_SERVICE_ID_WRITE_DATA_BY_IDENTIFIER,   UDS_SERVICE_TYPE_SID_DATA,    uds_write_data_by_id},
    [0x31] = {UDS_SERVICE_ID_ROUTINE_CONTROL,            UDS_SERVICE_TYPE_SID_SUB_DATA, uds_routine_control},
    [0x34] = {UDS_SERVICE_ID_REQUEST_DOWNLOAD,           UDS_SERVICE_TYPE_SID_DATA,     uds_request_download},
    [0x36] = {UDS_SERVICE_ID_TRANSFER_DATA,              UDS_SERVICE_TYPE_SID_DATA,     uds_transfer_data},
    [0x37] = {UDS_SERVICE_ID_REQUEST_TRANSFER_EXIT,      UDS_SERVICE_TYPE_SID_DATA,     uds_request_transfer_exit},
    [0x3E] = {UDS_SERVICE_ID_TESTER_PRESENT,             UDS_SERVICE_TYPE_SID_ONLY,    uds_tester_present},
    [0x85] = {UDS_SERVICE_ID_CONTROL_DTC_SETTING,        UDS_SERVICE_TYPE_SID_SUB,     uds_control_dtc_setting},
};
```

这会**破坏现有代码风格**（当前是逐个索引显式列出）。而且当前 `uds.c` 中 `uds_poll()` 的 `expected_type`/`handler` 从数组中直接取，未初始化槽自动为 0 = `UDS_SERVICE_TYPE_NONE` → 触发 NRC 0x11 (ServiceNotSupported)，逻辑正确。

**最安全的方式**：保留现有逐个列出的风格但改为使用命名初始化 + 省略未实现槽。C99 支持。

综上，将整个 `services_table[]` 替换为命名初始化版本。未初始化的槽自动为 `{0, UDS_SERVICE_TYPE_NONE, NULL}`。

- [ ] **Step 5: 实现 `uds_flash_reset_state()`**

在 services_table 之后添加：

```c
/**
 * @brief 复位刷写状态机到 IDLE
 */
void uds_flash_reset_state(void)
{
    g_flash_state        = UDS_FLASH_IDLE;
    g_flash_base_addr    = 0;
    g_flash_total_size   = 0;
    g_flash_write_offset = 0;
    g_block_seq_expected = 1;
}
```

- [ ] **Step 6: 实现 7 个 handler**

在 `uds_flash_reset_state()` 之后、EOF 之前，添加所有 handler 实现。

每个 handler 在各自的 section 块中，使用 `/* ==== UDS 0xXX 服务名 ==== */` 分隔。

**0x11 ECU Reset:**
```c
/****************************************************************************/
/*                      UDS 0x11 ECU Reset                                 */
/****************************************************************************/

uds_handler_result_t uds_ecu_reset(uds_request_t *req, uds_response_t *resp)
{
    if (req->sub_sid != 0x01 && req->sub_sid != 0x02 && req->sub_sid != 0x03)
    {
        uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        return UDS_HANDLER_NRC_SENT;
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = req->sub_sid;
    resp->data_len = 1;

    if (req->sub_sid == 0x01)
    {
        uds_port_ecu_reset();
    }

    return UDS_HANDLER_DONE;
}
```

**0x28 Communication Control:**
```c
/****************************************************************************/
/*                   UDS 0x28 Communication Control                        */
/****************************************************************************/

uds_handler_result_t uds_communication_control(uds_request_t *req, uds_response_t *resp)
{
    uint8_t comm_type;

    if (req->sub_sid != 0x00 && req->sub_sid != 0x01 && req->sub_sid != 0x03)
    {
        uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < 1)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    comm_type = req->data_ptr[0];

    if (comm_type < 0x01 || comm_type > 0x03)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = comm_type;
    resp->data_len = 1;

    uds_port_comm_control(req->sub_sid, comm_type);

    return UDS_HANDLER_DONE;
}
```

**0x31 Routine Control:**
```c
/****************************************************************************/
/*                    UDS 0x31 Routine Control                             */
/****************************************************************************/

uds_handler_result_t uds_routine_control(uds_request_t *req, uds_response_t *resp)
{
    uint16_t routine_id;
    uint8_t  i;
    const uds_routine_t *routine = NULL;

    if (req->sub_sid != 0x01)
    {
        if (req->sub_sid == 0x02 || req->sub_sid == 0x03)
        {
            uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        }
        else
        {
            uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        }
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < 2)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    routine_id = ((uint16_t)req->data_ptr[0] << 8) | (uint16_t)req->data_ptr[1];

    for (i = 0; i < routine_table_size; i++)
    {
        if (routine_table[i].routine_id == routine_id)
        {
            routine = &routine_table[i];
            break;
        }
    }

    if (routine == NULL)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    if (routine->session != UDS_SESSION_DEFAULT)
    {
        if (uds_get_session() != routine->session)
        {
            uds_send_negative_response(req->sid, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    if (routine->security != UDS_SEC_LEVEL_LOCKED)
    {
        if (uds_get_security_level() != routine->security)
        {
            uds_send_negative_response(req->sid, NRC_SECURITY_ACCESS_DENIED);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    {
        int ret;
        ret = routine->start(req->data_ptr + 2, req->data_len - 2);

        if (ret != 0)
        {
            uds_send_negative_response(req->sid, NRC_GENERAL_PROGRAMMING_FAILURE);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = req->sub_sid;
    resp->data_ptr[1] = (uint8_t)(routine_id >> 8);
    resp->data_ptr[2] = (uint8_t)(routine_id & 0xFF);
    resp->data_len = 3;

    /* EraseMemory 万能复位 */
    if (routine_id == 0xFF00)
    {
        g_flash_state        = UDS_FLASH_IDLE;
        g_flash_base_addr    = 0;
        g_flash_total_size   = 0;
        g_flash_write_offset = 0;
        g_block_seq_expected = 1;
    }
    /* CheckProgrammingDependencies */
    else if (routine_id == 0xFF01)
    {
        if (g_flash_state != UDS_FLASH_TRANSFER_COMPLETE)
        {
            uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
            return UDS_HANDLER_NRC_SENT;
        }
        g_flash_state = UDS_FLASH_VERIFIED;
    }

    return UDS_HANDLER_DONE;
}
```

**0x34 Request Download:**
```c
/****************************************************************************/
/*                   UDS 0x34 Request Download                             */
/****************************************************************************/

uds_handler_result_t uds_request_download(uds_request_t *req, uds_response_t *resp)
{
    uint8_t  dfi;
    uint8_t  alfi;
    uint8_t  addr_len;
    uint8_t  size_len;
    uint32_t memory_address;
    uint32_t memory_size;
    uint32_t max_block_size;
    uint32_t sector_size;
    uint8_t  idx;

    if (g_flash_state != UDS_FLASH_IDLE)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < 2)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    dfi  = req->data_ptr[0];
    alfi = req->data_ptr[1];

    if ((dfi & 0x0F) != 0x00)
    {
        /* 不支持压缩/加密 */
        uds_send_negative_response(req->sid, NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED);
        return UDS_HANDLER_NRC_SENT;
    }

    addr_len = (alfi >> 4) & 0x0F;
    size_len = alfi & 0x0F;

    if (addr_len == 0 || size_len == 0 || addr_len > 4 || size_len > 4)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < (uint16_t)(2 + addr_len + size_len))
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    /* 解析 memoryAddress（大端） */
    memory_address = 0;
    for (idx = 0; idx < addr_len; idx++)
    {
        memory_address = (memory_address << 8) | req->data_ptr[2 + idx];
    }

    /* 解析 memorySize（大端） */
    memory_size = 0;
    for (idx = 0; idx < size_len; idx++)
    {
        memory_size = (memory_size << 8) | req->data_ptr[2 + addr_len + idx];
    }

    if (memory_size == 0)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    g_flash_base_addr    = memory_address;
    g_flash_total_size   = memory_size;
    g_flash_write_offset = 0;
    g_block_seq_expected = 1;
    g_flash_state        = UDS_FLASH_DOWNLOAD_REQUESTED;

    /* 查询最大块大小（对齐粒度） */
    sector_size   = uds_port_flash_get_sector_size(memory_address);
    max_block_size = (sector_size > 0) ? sector_size : 4096;

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = 0x00;  /* lengthFormatIdentifier: 对齐=未指定, maxBlockLength 长度为 4 */
    resp->data_ptr[1] = (uint8_t)(max_block_size >> 24);
    resp->data_ptr[2] = (uint8_t)(max_block_size >> 16);
    resp->data_ptr[3] = (uint8_t)(max_block_size >> 8);
    resp->data_ptr[4] = (uint8_t)(max_block_size);
    resp->data_len = 5;

    return UDS_HANDLER_DONE;
}
```

**0x36 Transfer Data:**
```c
/****************************************************************************/
/*                     UDS 0x36 Transfer Data                              */
/****************************************************************************/

uds_handler_result_t uds_transfer_data(uds_request_t *req, uds_response_t *resp)
{
    uint8_t  block_seq;
    uint16_t payload_len;
    int      ret;

    if (g_flash_state != UDS_FLASH_DOWNLOAD_REQUESTED
        && g_flash_state != UDS_FLASH_TRANSFERRING)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < 1)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    block_seq   = req->data_ptr[0];
    payload_len = req->data_len - 1;

    if (block_seq == 0)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    if (block_seq != g_block_seq_expected)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
        return UDS_HANDLER_NRC_SENT;
    }

    if (g_flash_write_offset + payload_len > g_flash_total_size)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    ret = uds_port_flash_write(g_flash_base_addr + g_flash_write_offset,
                               req->data_ptr + 1, payload_len);

    if (ret != 0)
    {
        uds_send_negative_response(req->sid, NRC_GENERAL_PROGRAMMING_FAILURE);
        return UDS_HANDLER_NRC_SENT;
    }

    g_flash_write_offset += payload_len;

    g_block_seq_expected = (uint8_t)(block_seq + 1);
    if (g_block_seq_expected == 0)
    {
        g_block_seq_expected = 1;
    }

    g_flash_state = UDS_FLASH_TRANSFERRING;

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = block_seq;
    resp->data_len = 1;

    return UDS_HANDLER_DONE;
}
```

**0x37 Request Transfer Exit:**
```c
/****************************************************************************/
/*                  UDS 0x37 Request Transfer Exit                         */
/****************************************************************************/

uds_handler_result_t uds_request_transfer_exit(uds_request_t *req, uds_response_t *resp)
{
    uint32_t crc;

    if (g_flash_state != UDS_FLASH_TRANSFERRING)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
        return UDS_HANDLER_NRC_SENT;
    }

    (void)uds_port_flash_verify(g_flash_base_addr, g_flash_write_offset, &crc);

    g_flash_state = UDS_FLASH_TRANSFER_COMPLETE;

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_len = 0;

    return UDS_HANDLER_DONE;
}
```

**0x85 Control DTC Setting:**
```c
/****************************************************************************/
/*                  UDS 0x85 Control DTC Setting                           */
/****************************************************************************/

uds_handler_result_t uds_control_dtc_setting(uds_request_t *req, uds_response_t *resp)
{
    if (req->sub_sid != 0x01 && req->sub_sid != 0x02)
    {
        uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        return UDS_HANDLER_NRC_SENT;
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = req->sub_sid;
    resp->data_len = 1;

    uds_port_dtc_control(req->sub_sid);

    return UDS_HANDLER_DONE;
}
```

- [ ] **Step 7: 提交**

```bash
git add modules/can_bus/can_diag/uds/core/uds_service.c
git commit -m "feat(uds): uds_service.c 新增 7 个刷写 handler + routine_table + 状态机"
```

---

### Task 6: 更新 `core/uds.c` — 适配 uds_init 和状态机钩子

**文件:**
- 修改: `modules/can_bus/can_diag/uds/core/uds.c`

**接口:**
- 消费: Task 3 的 `uds_config_t`；Task 5 的 `uds_flash_reset_state()`；Task 1 的 `uds_port.h`
- 产出: 修改 `uds_init` → `int uds_init(const uds_config_t *config)`

- [ ] **Step 1: 添加 `#include "uds_port.h"`**

在现有 includes 后添加：

```c
#include "uds_port.h"
```

- [ ] **Step 2: 修改 `uds_init` 函数签名和实现**

将：
```c
int uds_init(uint32_t request_can_id, uint32_t response_can_id)
{

    if (g_initialized)
    {
        uds_deinit();
    }


    isotp_init_handle(&g_isotp_handle, request_can_id, response_can_id,
                          g_uds_send_buf, ISOTP_BUF_SIZE,
                          g_uds_recv_buf, ISOTP_BUF_SIZE);
    isotp_register_recv_cb(&g_isotp_handle, uds_on_recv);

    g_current_session        = UDS_SESSION_DEFAULT;
    g_current_security_level = UDS_SEC_LEVEL_LOCKED;
    g_request_flag            = 0;
    g_request_len             = 0;

    g_initialized = 1;
    return 0;
}
```

改为：

```c
int uds_init(const uds_config_t *config)
{
    uint32_t request_can_id;
    uint32_t response_can_id;

    if (g_initialized)
    {
        uds_deinit();
    }

    if (config != NULL)
    {
        request_can_id  = config->request_can_id;
        response_can_id = config->response_can_id;
    }
    else
    {
        request_can_id  = 0;
        response_can_id = 0;
    }

    isotp_init_handle(&g_isotp_handle, request_can_id, response_can_id,
                          g_uds_send_buf, ISOTP_BUF_SIZE,
                          g_uds_recv_buf, ISOTP_BUF_SIZE);
    isotp_register_recv_cb(&g_isotp_handle, uds_on_recv);

    g_current_session        = UDS_SESSION_DEFAULT;
    g_current_security_level = UDS_SEC_LEVEL_LOCKED;
    g_request_flag            = 0;
    g_request_len             = 0;

    g_initialized = 1;
    return 0;
}
```

- [ ] **Step 3: 在 `uds_set_session` 中添加刷写状态机复位**

在 `uds_set_session()` 函数中，`g_current_session = session;` 之后，切换到 Default Session 时增设 `uds_flash_reset_state()` 调用：

将：
```c
void uds_set_session(uds_session_t session)
{
    g_current_session = session;
    /* ISO 14229: switching to default session resets security level */
    if (session == UDS_SESSION_DEFAULT)
    {
        g_current_security_level = UDS_SEC_LEVEL_LOCKED;
    }
}
```

改为：

```c
void uds_set_session(uds_session_t session)
{
    g_current_session = session;
    /* ISO 14229: switching to default session resets security level */
    if (session == UDS_SESSION_DEFAULT)
    {
        g_current_security_level = UDS_SEC_LEVEL_LOCKED;
        uds_flash_reset_state();
    }
}
```

- [ ] **Step 4: 提交**

```bash
git add modules/can_bus/can_diag/uds/core/uds.c
git commit -m "feat(uds): uds.c 适配 uds_config_t，Default Session 时复位刷写状态机"
```

---

### 验收检查清单

实现完成后确认以下各项：

- [ ] `uds_init(NULL)` 行为与旧版一致（纯诊断模式）
- [ ] `uds_init(&config)` 正常初始化 ISO-TP（CAN ID 来自 config）
- [ ] `services_table` 共 134 个槽（`UDS_SERVICE_TABLE_SIZE = 0x86`），未实现槽自动为 `{0, NONE, NULL}`
- [ ] 0x10 → Programming Session → 0x34 → 0x36×N → 0x37 → 0x31(Check) → 0x11 流程顺畅
- [ ] 状态机违规返回 NRC 0x24（如在 IDLE 时调 0x36）
- [ ] 0x31 EraseMemory 任意状态可调，执行后状态回到 IDLE
- [ ] 0x36 blockSequenceCounter 校验：不匹配返回 NRC 0x24
- [ ] 0x10 切回 Default Session 时刷写状态机复位到 IDLE
- [ ] 0x11 HardReset (sub=0x01) 调用 `uds_port_ecu_reset()`
- [ ] 0x28/0x85 透传到 port 空函数
- [ ] 现有 5 个诊断服务行为无变化
