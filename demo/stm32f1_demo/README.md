# STM32F1 Demo 模块接入教程

本文说明如何把 **modules/** 下的模块接入 **demo/stm32f1_demo**。

这个工程是 **Keil MDK + STM32F1 HAL 的裸机工程**，不使用 ESP32 demo 的 **DEMO_MODULES**、**demo_modules.cmake** 或 **manifests/**。STM32 这边没有自动依赖展开，模块文件、include 路径、HAL 源文件和外设 handle 都需要在 Keil 工程里显式配置。

---

## 1. 和 ESP32 demo 的区别

- ESP32 demo 用 CMake 清单自动展开模块源文件和 include。
- STM32F1 demo 用 Keil 的文件组和 Include Paths，手动添加。
- ESP32 的 IDF 组件依赖写在 **PRIV_REQUIRES**。
- STM32 的 HAL 依赖通过“添加 HAL 源文件 + 打开 HAL 模块宏”完成。
- ESP32 的 **demo/common** 已经移到 **demo/esp32s3_demo/common**，只属于 ESP32 demo；STM32 不使用它。

对应关系：

- **DEMO_MODULES** → Keil 的 **Modules** 文件组
- **manifests/<模块>.cmake** → 没有，手动 Add Existing Files
- **DEMO_MODULE_INCLUDES** → Keil 的 **Include Paths**
- **PRIV_REQUIRES** → HAL 源文件 + **HAL_XXX_MODULE_ENABLED**
- **target_compile_definitions** → Keil 的 **Define** 字段，或 port 文件里的 **#ifndef**
- **idf.py build** → Keil Build 或 **build.ps1**

---

## 2. 工程结构

```text
demo/stm32f1_demo/
├── MDK-ARM/
│   └── stm32f1_demo.uvprojx      Keil 工程
├── Core/
│   ├── Inc/                      main.h、HAL 配置、中断头
│   └── Src/                      main、MSP、中断、system
├── app/
│   ├── include/app_main.h
│   └── src/app_main.c            应用入口
├── Drivers/
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/     HAL 源文件和头文件
├── stm32f1_demo.ioc              CubeMX 配置
└── build.ps1                     Keil 构建/烧录脚本
```

当前 Keil 工程里已经有一个模块组：

```text
Modules/utility
  └── ../../../modules/utility/core/utilities.c
```

Include Paths 当前包含：

```text
../Core/Inc
../app/include
../../../modules/utility/include
../Drivers/STM32F1xx_HAL_Driver/Inc
../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
../Drivers/CMSIS/Device/ST/STM32F1xx/Include
../Drivers/CMSIS/Include
```

---

## 3. 最快路径：utility 已经接好

**utility** 不依赖硬件，已经加入 Keil 的 **Modules/utility** 组和 Include Paths。

在 **app/src/app_main.c** 里直接包含公共头：

```c
#include "utilities.h"
```

然后调用：

```c
float duty = clamp_float(1.5f, 0.0f, 1.0f);
```

不需要新增 HAL 源文件，也不需要改 **stm32f1xx_hal_conf.h**。

可以把 **utility** 当作“模块已经配置好”的参考。

---

## 4. 完整示例：添加 w25q80（SPI1）

**w25q80** 已经有 STM32F1 port，但还没有加入 demo。

模块结构：

```text
modules/w25q80/
├── include/
│   └── w25q80.h
├── core/
│   └── w25q80.c
└── port/
    ├── w25q80_port.h
    └── stm32_w25q80_port.c
```

STM32 port 默认配置：

```c
W25Q80_STM32_SPI_HANDLE   = &hspi1
W25Q80_STM32_CS_GPIO_PORT = GPIOA
W25Q80_STM32_CS_GPIO_PIN  = GPIO_PIN_4
W25Q80_STM32_TIMEOUT_MS   = 100
```

也就是 SPI1 + PA4 软件 CS。

### 4.1 把模块源文件加入 Keil

打开：

```text
demo/stm32f1_demo/MDK-ARM/stm32f1_demo.uvprojx
```

在 Keil 的 Project 窗口：

1. 右键 **Modules/utility**，选择 **Add Group...**，组名写 **Modules/w25q80**。
2. 右键 **Modules/w25q80**，选择 **Add Existing Files to Group 'Modules/w25q80'...**。
3. 添加：

```text
modules/w25q80/core/w25q80.c
modules/w25q80/port/stm32_w25q80_port.c
```

不要添加 **esp32_w25q80_port.c**，那是 ESP32 的 port。

Keil 工程位于 **MDK-ARM**，所以文件路径最终类似：

```text
../../../modules/w25q80/core/w25q80.c
../../../modules/w25q80/port/stm32_w25q80_port.c
```

### 4.2 加 Include Paths

打开 **Options for Target** → **C/C++** → **Include Paths**，新增：

```text
../../../modules/w25q80/include
../../../modules/w25q80/port
```

**include/** 给应用包含 **w25q80.h**，**port/** 给 **w25q80.c** 包含 **w25q80_port.h**。

原有的 **../../../modules/utility/include** 保留。

### 4.3 打开 HAL SPI，并加入 SPI 源文件

当前 **stm32f1xx_hal_conf.h** 里 SPI 是关闭的。打开：

```text
demo/stm32f1_demo/Core/Inc/stm32f1xx_hal_conf.h
```

把：

```c
/*#define HAL_SPI_MODULE_ENABLED   */
```

改成：

```c
#define HAL_SPI_MODULE_ENABLED
```

然后把 HAL SPI 源文件加入 Keil 的 **Drivers/STM32F1xx_HAL_Driver** 组：

```text
demo/stm32f1_demo/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c
```

F1 HAL 只有 **stm32f1xx_hal_spi.c**，没有单独的 **spi_ex.c**。

### 4.4 配置 hspi1 和 SPI1 GPIO

在 **Core/Src/main.c** 增加全局 handle：

```c
SPI_HandleTypeDef hspi1;
```

在 **Core/Inc/main.h** 增加：

```c
extern SPI_HandleTypeDef hspi1;
```

在 **Core/Src/main.c** 增加 SPI1 初始化：

```c
static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}
```

如果这个函数定义放在 **main()** 后面，记得在文件顶部补原型：

    static void MX_SPI1_Init(void);

在 **main()** 里调用：

```c
MX_GPIO_Init();
MX_USART1_UART_Init();
MX_SPI1_Init();
```

在 **Core/Src/stm32f1xx_hal_msp.c** 增加 SPI1 的 GPIO 配置：

```c
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hspi->Instance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}
```

PA4 是 CS，由 **w25q80** port 自己配成 GPIO 输出，不要在这里配成 SPI 复用。

**SPI_BAUDRATEPRESCALER_4** 在 APB2 72MHz 下约为 18MHz。W25Q80 可以先从 18MHz 验证，稳定后再提高。

### 4.5 给 port 补 hspi1 声明，并打开 DWT

**modules/w25q80/port/stm32_w25q80_port.c** 直接用了 **&hspi1**，但它没有包含 **main.h**。在这个文件顶部 include 之后加一行：

```c
extern SPI_HandleTypeDef hspi1;
```

否则编译会报 **hspi1 undeclared**。

这个 port 的微秒延时使用 DWT：

```c
uint32_t start = DWT->CYCCNT;
```

当前 port 没有显式打开 DWT cycle counter。如果 DWT 没开，**w25q80_init()** 里的延时可能卡死。

在 **app_main()** 最开始加：

```c
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

这是 Cortex-M3 的 DWT 使能，不需要额外 HAL 模块。

### 4.6 在 app_main.c 中使用

在 **app/src/app_main.c** 里加入：

```c
#include "w25q80.h"
```

然后在 **app_main()** 里、进入 LED 循环之前调用：

```c
w25q80_info_t info;
uint8_t wr[16];
uint8_t rd[16];

CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

if (w25q80_init(NULL) != 0)
{
    /* 初始化失败：检查 hspi1、HAL_SPI、CS 和接线 */
    while (1)
    {
    }
}

if (w25q80_read_jedec_id(&info) == 0)
{
    /* JEDEC: EF 13 14 */
}

for (uint32_t i = 0; i < sizeof(wr); i++)
{
    wr[i] = (uint8_t)i;
}

w25q80_sector_erase(0x000000);
w25q80_write(0x000000, wr, sizeof(wr));
w25q80_read(0x000000, rd, sizeof(rd));
```

要点：

- 应用只包含 **w25q80.h**，不要包含 **w25q80_port.h**。
- 应用不调用 **w25q80_port_get_driver()**；**w25q80_init()** 内部会调用它。
- W25Q80 是外部 NOR Flash，写之前必须先擦除。
- 示例地址 **0x000000** 会擦除第一个 4KB 扇区，确认这块外部 Flash 没有需要保留的数据。
- **w25q80_init()** 返回 **-4** 通常表示 port driver 无效或 port 的 **init()** 失败。

### 4.7 构建和烧录

Keil 里直接：

```text
Project -> Build Target
Flash -> Download
```

或者用脚本：

```powershell
.\demo\stm32f1_demo\build.ps1 -Action build
.\demo\stm32f1_demo\build.ps1 -Action flash
```

如果程序卡在 **w25q80_init()**，优先检查 DWT 是否使能。

如果 JEDEC 读到 **FF FF FF**，检查 MISO、CS、供电和地线，先不要继续擦写。

---

## 5. 通用流程：任何 STM32 模块

### 5.1 确认模块有 F1 port

检查：

```text
modules/<模块>/port/stm32_*.c
```

如果只有 **esp32_*.c**，或者 port 里包含 **stm32f4xx_hal.h**、**stm32h7xx_hal.h**，不能直接用于 F1。

### 5.2 把源文件加进 Keil

添加：

```text
modules/<模块>/core/*.c
modules/<模块>/port/stm32_*.c
```

不要添加其他平台的 port。

### 5.3 加 Include Paths

添加：

```text
../../../modules/<模块>/include
../../../modules/<模块>/port
```

如果模块依赖另一个仓库模块，把依赖模块的 include 路径也加上。

### 5.4 加 HAL 源文件

按模块使用的外设添加：

- SPI → **stm32f1xx_hal_spi.c**
- I2C → **stm32f1xx_hal_i2c.c**
- CAN → **stm32f1xx_hal_can.c**
- TIM → **stm32f1xx_hal_tim.c** 和 **stm32f1xx_hal_tim_ex.c**
- ADC → **stm32f1xx_hal_adc.c** 和 **stm32f1xx_hal_adc_ex.c**
- USART/UART → **stm32f1xx_hal_uart.c**（当前已有）
- GPIO/RCC/DMA/FLASH/PWR/CORTEX/EXTI → 当前工程已有

### 5.5 打开 HAL 模块宏

在 **Core/Inc/stm32f1xx_hal_conf.h** 里取消对应注释：

```c
#define HAL_SPI_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_CAN_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
```

只打开实际用到的模块。宏和源文件要同时存在：

- 只打开宏，不加源文件 → 链接时找不到 HAL 函数。
- 只加源文件，不打开宏 → 源文件内容可能被条件编译掉。

### 5.6 提供 port 需要的 HAL handle 和 MSP

常见对应关系：

- **w25q80** → **hspi1** + **HAL_SPI_MspInit**
- **log-lib** → **huart1**（模板已有）
- **eeprom**/**swi2c** → I2C handle 或软件 I2C 的 GPIO
- **ina226**/**bq40z80** → I2C handle
- **key** → GPIO 输入

如果 port 文件直接用了 **hspi1**、**huart1**、**hi2c1** 这类全局 handle，记得在 port 文件里补 **extern**，或者让它包含 **main.h**。

### 5.7 在 app_main.c 中调用

只包含模块的公共头：

```c
#include "w25q80.h"
```

不要包含 **core/** 或 **port/** 的内部头文件，除非模块设计明确要求。

### 5.8 手动处理模块依赖

Keil 没有 ESP32 的 **DEMO_MODULE_DEPENDS** 自动展开。

如果模块 A 依赖模块 B：

1. 把 A 和 B 的 **core/*.c** 都加入 Keil。
2. 把 A 和 B 的 **port/stm32_*.c** 都加入 Keil。
3. 把 A 和 B 的 **include/**、**port/** 都加入 Include Paths。
4. 如果 B 还需要额外 HAL 外设，按 5.4 和 5.5 配置。

漏掉依赖时，链接器会报 **undefined reference**。

### 5.9 覆盖硬件宏

port 文件里的硬件参数一般写成：

```c
#ifndef W25Q80_STM32_SPI_HANDLE
#define W25Q80_STM32_SPI_HANDLE (&hspi1)
#endif
```

可以在 Keil 的 **Options for Target → C/C++ → Define** 里覆盖：

```text
W25Q80_STM32_SPI_HANDLE=&hspi2
W25Q80_STM32_CS_GPIO_PORT=GPIOB
W25Q80_STM32_CS_GPIO_PIN=GPIO_PIN_12
```

带 **&** 或括号的宏在 Keil Define 里可能不好写，直接改 port 文件的 **#ifndef** 默认值通常更简单。

### 5.10 FreeRTOS

当前 STM32F1 模板是裸机，没有 FreeRTOS。

如果模块 core 直接包含 **freertos/FreeRTOS.h**、**task.h**，或者使用 **xTask**、**xQueue**，需要先把 FreeRTOS 内核和 port 加入工程，不能只加模块文件。

---

## 6. 用 CubeMX 生成外设初始化

工程里已经有：

```text
demo/stm32f1_demo/stm32f1_demo.ioc
```

如果不想手写 SPI1 初始化，可以：

1. 用 CubeMX 打开 **stm32f1_demo.ioc**。
2. 启用 SPI1：
   - Full-Duplex Master
   - 8-bit
   - CPOL Low / CPHA 1Edge
   - NSS Software
   - Prescaler 4（约 18MHz）
   - PA5 SCK / PA6 MISO / PA7 MOSI
   - PA4 配成 GPIO Output 作为 CS
3. 生成代码。
4. 回到 Keil，检查 **Modules** 组和 Include Paths 是否还在。

CubeMX 会生成 **hspi1**、**MX_SPI1_Init**、**HAL_SPI_MspInit**、**HAL_SPI_MODULE_ENABLED** 和 HAL SPI 源文件，省掉第 4.3、4.4 步的大部分手工代码。

注意：CubeMX 重新生成 **stm32f1_demo.uvprojx** 时，自定义的 **Modules** 组有可能被覆盖，需要重新添加模块文件和 Include Paths。

---

## 7. 当前 STM32 port 适配情况

- **F1 HAL，可直接参考**：
  - **log-lib/port/stm32_logger_port.c**
  - **w25q80/port/stm32_w25q80_port.c**
- **F4 HAL，不能直接用于 F1**：
  - **swi2c/port/stm32_i2c_port.c**
  - **encoder/port/stm32_encoder_port.c**
  - **ina226/port/stm32_ina226_port.c**
  - **bq40z80/port/stm32_bq40z80_port.c**
  - **key/port/stm32_key_port.c**（F4 示例，头文件是注释状态）
- **H7 HAL，不能直接用于 F1**：
  - **ws2812/port/stm32_gpio.c**
  - **ws2812/port/stm32_timer+dma.c**
- **USB Host/FatFS**：
  - **udisk/port/stm32_udisk_port.c** 需要 STM32 USB Host 和 FatFS，不是当前模板的一部分。
- **头文件接口示例**：
  - **modules/port demo** 下的 i2c/spi/uart/can 只有接口和空实现，不是可编译驱动。

已知需要的小修正：

- **w25q80/port/stm32_w25q80_port.c**：
  - 需要补 **extern SPI_HandleTypeDef hspi1;**
  - 使用 DWT 前需要打开 DWT cycle counter。
- **log-lib/core/logger.c**：
  - 使用了 **bool**，但没有包含 **stdbool.h**，编译前需要补 **#include <stdbool.h>**。
- **log-lib/port/stm32_logger_port.c**：
  - 使用了 **huart1**，如果 port 文件没有包含 **main.h**，需要补 **extern UART_HandleTypeDef huart1;**

---

## 8. 常见错误

- **hspi1 undeclared**
  → 在 **stm32_w25q80_port.c** 里加 **extern SPI_HandleTypeDef hspi1;**，或让它包含 **main.h**。

- **HAL_SPI_MODULE_ENABLED 未定义 / HAL_SPI_Init 找不到**
  → 打开 **stm32f1xx_hal_conf.h** 里的 **HAL_SPI_MODULE_ENABLED**。

- **HAL_SPI_Init 链接失败**
  → 把 **stm32f1xx_hal_spi.c** 加进 Keil 的 HAL 组。

- **SPI 没有波形**
  → 检查 **HAL_SPI_MspInit** 是否配置 PA5/PA6/PA7，SPI1 时钟是否使能。

- **CS 一直不拉低**
  → **w25q80** port 自己配置 PA4 为 GPIO 输出；检查 PA4 是否被 CubeMX 配成了 SPI NSS 复用。

- **JEDEC 读到 FF FF FF**
  → 检查 MISO、CS、供电和地线；先不要继续擦写。

- **程序卡在 w25q80_init()**
  → 检查 DWT 是否使能，或者把 port 的 **delay_us** 改成基于定时器/NOP 的实现。

- **undefined reference to w25q80_port_get_driver**
  → **stm32_w25q80_port.c** 没有加入 Keil。

- **w25q80.h: No such file or directory**
  → Include Paths 少了 **../../../modules/w25q80/include**。

- **w25q80_port.h: No such file or directory**
  → Include Paths 少了 **../../../modules/w25q80/port**。

- **swi2c/encoder/ina226/bq40z80 编译报 F4 头文件**
  → 这些 port 目前是 **stm32f4xx_hal.h**，不能直接用于 F1；需要先写 F1 port。

- **模块 core 使用 FreeRTOS**
  → 当前模板是裸机；需要另外把 FreeRTOS 源文件和 port 加进工程。

- **L6218E: Undefined symbol**
  → 通常是模块源文件、HAL 源文件或依赖模块没有加入 Keil。

---

## 9. 新增模块检查清单

- [ ] **modules/<模块>/include/<模块>.h** 存在，是公共 API。
- [ ] **modules/<模块>/core/*.c** 存在。
- [ ] **modules/<模块>/port/stm32_*.c** 存在，且是 F1 HAL。
- [ ] Keil 的 **Modules/<模块>** 组包含 core 和 STM32 port 的 .c。
- [ ] Include Paths 包含 **modules/<模块>/include** 和 **modules/<模块>/port**。
- [ ] 对应 HAL 源文件已加入 Keil 的 HAL 组。
- [ ] **stm32f1xx_hal_conf.h** 已打开对应 **HAL_XXX_MODULE_ENABLED**。
- [ ] port 需要的 HAL handle 已定义并初始化。
- [ ] port 需要的 **HAL_XXX_MspInit** 已配置 GPIO/时钟。
- [ ] port 文件需要的 **extern** handle 已补齐。
- [ ] **app_main.c** 包含公共头并调用了至少一个公开 API。
- [ ] 模块依赖的其他仓库模块文件也已加入。
- [ ] Keil Build 无错误，烧录后功能正常。

---

## 10. 构建和烧录

Keil 里：

```text
Project -> Build Target
Flash -> Download
```

或者：

```powershell
.\demo\stm32f1_demo\build.ps1 -Action build
.\demo\stm32f1_demo\build.ps1 -Action flash
```

查看构建日志：

```text
demo/stm32f1_demo/MDK-ARM/build.log
```

---

## 11. 一句话总结

STM32F1 demo 不使用 ESP32 的 **DEMO_MODULES** 和 manifest。添加模块时，把 **core/*.c** 和 **port/stm32_*.c** 加进 Keil 的 **Modules** 组，把 **include/** 和 **port/** 加进 Include Paths，把需要的 HAL 源文件加进 HAL 组并打开对应 **HAL_XXX_MODULE_ENABLED**，再提供 port 需要的 handle 和 MSP。最后在 **app_main.c** 中包含公共头并调用 API。
