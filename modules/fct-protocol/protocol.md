# FCT 工装上位机通信协议

- 版本：v0.4
- 日期：2026-11-27
- 方向：工装 → 上位机，单向
- 关联需求：[软件需求](requirements.md)

本文只定义 USB 虚拟串口上的交互数据格式和消息语义，不定义上位机界面、CSV 文件格式或异常处理策略；这些内容见需求文档。

## 1. 串口参数

| 项目   | 约定              |
| ------ | ----------------- |
| 接口   | USB 虚拟串口 COMx |
| 波特率 | 115200            |
| 数据位 | 8                 |
| 停止位 | 1                 |
| 校验位 | None              |
| 流控   | None              |

- 上位机只读接收，不向工装发送任何数据。
- 一台上位机只接一台工装。

## 2. 帧格式

- 文本行协议，7-bit 可打印 ASCII。
- 一条消息一行，以 CRLF 结束；接收端兼容 LF。
- 所有消息以 #FCT, 开头；其它串口输出不属于本协议。
- 字段用半角逗号分隔，逗号两边不加空格。
- 关键字、类型、级别区分大小写，统一大写。

消息信封：

```text
#FCT,1,<fctType>,<seq>,<TYPE>[,<payload>...]
```

| 字段    | 说明                                                                                                         |
| ------- | ------------------------------------------------------------------------------------------------------------ |
| #FCT    | 固定前缀                                                                                                     |
| 1       | 协议版本，当前只支持 1                                                                                       |
| fctType | FCT 型号，uint8_t，十进制 0–255；0 保留为未知，1–255 由项目分配                                            |
| seq     | 工装消息序号，十进制无符号整数；上电后从 0 开始，每次 START 重置为 0，START 自身使用 0，之后每发一条消息加一 |
| TYPE    | START / LOG / RESULT / END / HB                                                                              |

- 同一台工装的 fctType 固定；一次测试内不应变化。
- seq 只在单轮 START → END 内有递增意义；跨轮次和 IDLE 心跳不保证唯一。

### 2.1 型号编码

| fctType | 型号      |
| ------- | --------- |
| 0       | 保留/未知 |
| 1       | 待定      |
| 2       | 待定      |
| 3       | 待定      |

具体型号名称和编码由现场分配后补入本表。

## 3. 消息定义

### 3.1 START

```text
#FCT,1,<fctType>,<seq>,START
```

- 一次测试开始。
- 无附加字段。
- 一次测试只应发送一条 START。
- START 使用 seq=0，并把本轮序号重置为 0。

### 3.2 LOG

```text
#FCT,1,<fctType>,<seq>,LOG,INFO,<text>
#FCT,1,<fctType>,<seq>,LOG,WARN,<text>
#FCT,1,<fctType>,<seq>,LOG,ERROR,<text>
#FCT,1,<fctType>,<seq>,LOG,DEBUG,<text>
```

- 测试过程日志。
- 只应出现在 START 与 RESULT 之间；RESULT 之后不再发送 LOG。
- 级别只允许 INFO / WARN / ERROR / DEBUG。
- text 是最后一个字段，可以包含逗号。
- text 不能包含 CR、LF 或其它控制字符；需要多行时拆成多条 LOG。
- text 只允许可打印 ASCII，不允许中文或非 ASCII 字符。

### 3.3 RESULT

```text
#FCT,1,<fctType>,<seq>,RESULT,OK,0
#FCT,1,<fctType>,<seq>,RESULT,NG,12
```

- 本次测试的最终结论。
- 结论只允许 OK 或 NG。
- 错误码为十进制 uint16（0–65535）。
- 0 固定表示成功；非 0 表示错误类型，具体含义按 fctType 由现场定义。
- 发送端应保证 OK 对应 0、NG 对应非 0；接收端当前不校验矛盾组合，按原样记录。
- 一次测试只应发送一条 RESULT。
- 失败细节仍由 LOG 表达，RESULT 只携带错误码，不携带文字说明。

### 3.4 END

```text
#FCT,1,<fctType>,<seq>,END
```

- 一次测试结束。
- 无附加字段。
- 一次测试只应发送一条 END。

### 3.5 HB

```text
#FCT,1,<fctType>,<seq>,HB,IDLE
#FCT,1,<fctType>,<seq>,HB,TESTING
```

- 心跳消息，每 2 秒发送一次。
- IDLE：当前没有测试。
- TESTING：当前处于 START 与 END 之间。
- 心跳不携带测试结果或日志内容。
- START 前、END 后的 IDLE 心跳也使用当前 seq 计数，但不属于某一轮测试。

## 4. 正常时序

```text
START (seq=0)
  LOG*
  RESULT
END
```

- 每次 START 将 seq 重置为 0。
- LOG 只应出现在 START 与 RESULT 之间；RESULT 之后不再发送 LOG。
- HB 独立于测试时序，每 2 秒发送一次；START 与 END 之间的 HB 也占用 seq。
- 所有消息按同一串口顺序发送。

## 5. 示例

第一轮测试（示例省略了测试期间的心跳）：

```text
#FCT,1,1,0,START
#FCT,1,1,1,LOG,INFO,power on self test start
#FCT,1,1,2,LOG,INFO,measure 3.3V = 3.31
#FCT,1,1,3,LOG,ERROR,measure 5V = 4.62, out of range
#FCT,1,1,4,RESULT,NG,12
#FCT,1,1,5,END
#FCT,1,1,6,HB,IDLE
```

下一轮测试，START 重新从 seq=0 开始：

```text
#FCT,1,1,0,START
#FCT,1,1,1,LOG,INFO,next board test start
```

# FCT 工装上位机通信协议

- 版本：v0.3
- 日期：2026-11-26
- 方向：工装 → 上位机，单向
- 关联需求：[软件需求](requirements.md)

本文只定义 USB 虚拟串口上的交互数据格式和消息语义，不定义上位机界面、CSV 文件格式或异常处理策略；这些内容见需求文档。

## 1. 串口参数

| 项目   | 约定              |
| ------ | ----------------- |
| 接口   | USB 虚拟串口 COMx |
| 波特率 | 115200            |
| 数据位 | 8                 |
| 停止位 | 1                 |
| 校验位 | None              |
| 流控   | None              |

- 上位机只读接收，不向工装发送任何数据。
- 一台上位机只接一台工装。

## 2. 帧格式

- 文本行协议，7-bit 可打印 ASCII。
- 一条消息一行，以 CRLF 结束；接收端兼容 LF。
- 所有消息以 #FCT, 开头；其它串口输出不属于本协议。
- 字段用半角逗号分隔，逗号两边不加空格。
- 关键字、类型、级别区分大小写，统一大写。

消息信封：

```text
#FCT,1,<fctType>,<seq>,<TYPE>[,<payload>...]
```

| 字段    | 说明                                                                                                         |
| ------- | ------------------------------------------------------------------------------------------------------------ |
| #FCT    | 固定前缀                                                                                                     |
| 1       | 协议版本，当前只支持 1                                                                                       |
| fctType | FCT 型号，uint8_t，十进制 0–255；0 保留为未知，1–255 由项目分配                                            |
| seq     | 工装消息序号，十进制无符号整数；上电后从 0 开始，每次 START 重置为 0，START 自身使用 0，之后每发一条消息加一 |
| TYPE    | START / LOG / RESULT / END / HB                                                                              |

- 同一台工装的 fctType 固定；一次测试内不应变化。
- seq 只在单轮 START → END 内有递增意义；跨轮次和 IDLE 心跳不保证唯一。

### 2.1 型号编码

| fctType | 型号      |
| ------- | --------- |
| 0       | 保留/未知 |
| 1       | 待定      |
| 2       | 待定      |
| 3       | 待定      |

具体型号名称和编码由现场分配后补入本表。

## 3. 消息定义

### 3.1 START

```text
#FCT,1,<fctType>,<seq>,START
```

- 一次测试开始。
- 无附加字段。
- 一次测试只应发送一条 START。
- START 使用 seq=0，并把本轮序号重置为 0。

### 3.2 LOG

```text
#FCT,1,<fctType>,<seq>,LOG,INFO,<text>
#FCT,1,<fctType>,<seq>,LOG,WARN,<text>
#FCT,1,<fctType>,<seq>,LOG,ERROR,<text>
#FCT,1,<fctType>,<seq>,LOG,DEBUG,<text>
```

- 测试过程日志。
- 只应出现在 START 与 RESULT 之间；RESULT 之后不再发送 LOG。
- 级别只允许 INFO / WARN / ERROR / DEBUG。
- text 是最后一个字段，可以包含逗号。
- text 不能包含 CR、LF 或其它控制字符；需要多行时拆成多条 LOG。
- text 只允许可打印 ASCII，不允许中文或非 ASCII 字符。

### 3.3 RESULT

```text
#FCT,1,<fctType>,<seq>,RESULT,OK
#FCT,1,<fctType>,<seq>,RESULT,NG
```

- 本次测试的最终结论。
- 只允许 OK 或 NG。
- 一次测试只应发送一条 RESULT。
- 失败细节由 LOG 表达，不在 RESULT 中携带。

### 3.4 END

```text
#FCT,1,<fctType>,<seq>,END
```

- 一次测试结束。
- 无附加字段。
- 一次测试只应发送一条 END。

### 3.5 HB

```text
#FCT,1,<fctType>,<seq>,HB,IDLE
#FCT,1,<fctType>,<seq>,HB,TESTING
```

- 心跳消息，每 2 秒发送一次。
- IDLE：当前没有测试。
- TESTING：当前处于 START 与 END 之间。
- 心跳不携带测试结果或日志内容。
- START 前、END 后的 IDLE 心跳也使用当前 seq 计数，但不属于某一轮测试。

## 4. 正常时序

```text
START (seq=0)
  LOG*
  RESULT
END
```

- 每次 START 将 seq 重置为 0。
- LOG 只应出现在 START 与 RESULT 之间；RESULT 之后不再发送 LOG。
- HB 独立于测试时序，每 2 秒发送一次；START 与 END 之间的 HB 也占用 seq。
- 所有消息按同一串口顺序发送。

## 5. 示例

第一轮测试（示例省略了测试期间的心跳）：

```text
#FCT,1,1,0,START
#FCT,1,1,1,LOG,INFO,power on self test start
#FCT,1,1,2,LOG,INFO,measure 3.3V = 3.31
#FCT,1,1,3,LOG,ERROR,measure 5V = 4.62, out of range
#FCT,1,1,4,RESULT,NG
#FCT,1,1,5,END
#FCT,1,1,6,HB,IDLE
```

下一轮测试，START 重新从 seq=0 开始：

```text
#FCT,1,1,0,START
#FCT,1,1,1,LOG,INFO,next board test start
```
