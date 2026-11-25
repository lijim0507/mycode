# ESP32-S3 Demo 模块接入教程

本文说明如何把 **modules/** 下的模块接入 **demo/esp32s3_demo**。重点是 CMake 配置：模块名写在哪里、模块清单怎么写、ESP-IDF 依赖加在哪里，以及 CMake 展开后如何进入编译和链接。

> 这里的 **common** 是 ESP32-S3 demo 专用，放在 **demo/esp32s3_demo/common/**。STM32F1 demo 是 Keil 工程，不使用这些 CMake 清单。

当前 **demo/esp32s3_demo/main/CMakeLists.txt** 的 **DEMO_MODULES** 是空的。只有列在 **DEMO_MODULES** 里的模块会参与编译。

---

## 1. CMake 文件分工

```text
demo/esp32s3_demo/CMakeLists.txt
  └─ 工程入口：include IDF 的 project.cmake，project(esp32s3_demo)
     通常不用改

demo/esp32s3_demo/main/CMakeLists.txt
  └─ 本 demo 的模块选择 + IDF 组件注册
     加模块时改这里

demo/esp32s3_demo/common/demo_modules.cmake
  └─ 通用展开器：读 DEMO_MODULES、加载 manifests、生成 DEMO_SRCS / DEMO_INC_DIRS
     不要为单个模块改它

demo/esp32s3_demo/common/manifests/<模块>.cmake
  └─ 单个模块的 CMake 清单：源文件、include、仓库内依赖
     加模块时新建或修改
```

**manifests/*.cmake** 不是独立工程，不需要 **cmake_minimum_required()**、**project()** 或 **add_subdirectory()**。它们只是被 **include()** 执行的脚本片段。

模块名不需要写进 **demo_modules.cmake**。作为顶层模块，模块名写在 **DEMO_MODULES**；作为依赖，模块名写在另一个模块清单的 **DEMO_MODULE_DEPENDS** 里。

---

## 2. 已有清单的模块

以 **ws2812** 为例。它已经有 **demo/esp32s3_demo/common/manifests/ws2812.cmake**、核心源码和 **port/esp32_rmt.c**。

只改 **demo/esp32s3_demo/main/CMakeLists.txt**：

```cmake
set(DEMO_PLATFORM "esp32")

set(DEMO_MODULES
    ws2812
)

include("${CMAKE_CURRENT_LIST_DIR}/../common/demo_modules.cmake")

idf_component_register(
    SRCS
        "main.c"
        ${DEMO_SRCS}
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        driver
        esp_timer
)

target_include_directories(${COMPONENT_LIB} PRIVATE ${DEMO_INC_DIRS})
```

然后在 **main.c** 里：

```c
#include "ws2812.h"

void app_main(void)
{
    ws2812_init(8);
    ws2812_set_all_color(WS2812_COLOR_RED, 255.0f);
    ws2812_show();
}
```

**ws2812** 的 ESP32 port 默认使用 **GPIO_NUM_2**。如果实际灯带引脚不同，在 **idf_component_register** 后面覆盖：

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WS2812_ESP32_GPIO_NUM=GPIO_NUM_48
)
```

---

## 3. 没有清单的模块：w25q80 完整示例

**w25q80** 已经有 ESP32 port，但还没有 demo 清单：

```text
modules/w25q80/
├── include/
│   └── w25q80.h
├── core/
│   └── w25q80.c
└── port/
    ├── w25q80_port.h
    └── esp32_w25q80_port.c
```

ESP32 port 默认配置：

```c
W25Q80_ESP32_SPI_HOST   = SPI3_HOST
W25Q80_ESP32_MOSI_GPIO  = GPIO_NUM_23
W25Q80_ESP32_MISO_GPIO  = GPIO_NUM_19
W25Q80_ESP32_CLK_GPIO   = GPIO_NUM_18
W25Q80_ESP32_CS_GPIO    = GPIO_NUM_5
W25Q80_ESP32_CLK_SPEED  = 40000000
```

实际接线不同就覆盖这些宏。ESP32-S3 上如果 **SPI3_HOST** 不可用，通常改成 **SPI2_HOST**。

### 3.1 新建模块清单

新建 **demo/esp32s3_demo/common/manifests/w25q80.cmake**：

```cmake
set(DEMO_MODULE_FILES
    "${DEMO_MODULE_ROOT}/w25q80/core/w25q80.c"
)

if(DEMO_PLATFORM STREQUAL "esp32")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/w25q80/port/esp32_w25q80_port.c"
    )
elseif(DEMO_PLATFORM STREQUAL "stm32f1")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/w25q80/port/stm32_w25q80_port.c"
    )
else()
    message(FATAL_ERROR "w25q80: 未适配平台 ${DEMO_PLATFORM}")
endif()

set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/w25q80/include"
    "${DEMO_MODULE_ROOT}/w25q80/port"
)

set(DEMO_MODULE_DEPENDS "")
```

三个变量只做三件事：

- **DEMO_MODULE_FILES**：这个模块要编译哪些 .c。**core/w25q80.c** 是核心逻辑，**port/esp32_w25q80_port.c** 是 ESP32 平台实现。
- **DEMO_MODULE_INCLUDES**：这个模块需要哪些头文件目录。**include/** 放公共头 **w25q80.h**，**port/** 放 **w25q80_port.h**，因为 **w25q80.c** 会包含它。
- **DEMO_MODULE_DEPENDS**：这个模块依赖仓库内哪些其他模块。**w25q80** 不依赖其他仓库模块，所以为空。

路径统一用 **${DEMO_MODULE_ROOT}** 拼绝对路径。不要写 **../../modules/...** 这种相对路径，换 demo 或换构建目录时容易出错。

### 3.2 修改 main/CMakeLists.txt

```cmake
set(DEMO_PLATFORM "esp32")

set(DEMO_MODULES
    w25q80
)

include("${CMAKE_CURRENT_LIST_DIR}/../common/demo_modules.cmake")

idf_component_register(
    SRCS
        "main.c"
        ${DEMO_SRCS}
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        driver
)

target_include_directories(${COMPONENT_LIB} PRIVATE ${DEMO_INC_DIRS})
```

顺序不能反：

1. 先 **set(DEMO_PLATFORM ...)**；
2. 再 **set(DEMO_MODULES ...)**；
3. 再 **include(...demo_modules.cmake)**；
4. 再 **idf_component_register(...)**；
5. 最后 **target_include_directories(...)**。

原因：

- **demo_modules.cmake** 需要读 **DEMO_PLATFORM** 和 **DEMO_MODULES**，所以必须在它们之后 include。
- **idf_component_register** 需要读 **DEMO_SRCS**，所以必须在 include 之后。
- **target_include_directories** 需要 **${COMPONENT_LIB}**，而它由 **idf_component_register** 创建，所以必须放在后面。

### 3.3 加 ESP-IDF 组件依赖

**w25q80** 的 ESP32 port 使用了：

```c
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```

所以 **PRIV_REQUIRES** 至少要有 **driver**。如果编译找不到 **esp_rom_sys.h**，补 **esp_rom**；找不到 FreeRTOS 头文件，补 **freertos**。

### 3.4 覆盖硬件宏

引脚或 SPI host 不匹配时，优先在 **main/CMakeLists.txt** 里覆盖，不要改 core：

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    W25Q80_ESP32_SPI_HOST=SPI2_HOST
    W25Q80_ESP32_MOSI_GPIO=GPIO_NUM_11
    W25Q80_ESP32_MISO_GPIO=GPIO_NUM_13
    W25Q80_ESP32_CLK_GPIO=GPIO_NUM_12
    W25Q80_ESP32_CS_GPIO=GPIO_NUM_10
    W25Q80_ESP32_CLK_SPEED=20000000
)
```

这段必须放在 **idf_component_register** 之后。上面的引脚只是示例，按实际接线改。

如果 SPI 总线已经被其他模块初始化，**spi_bus_initialize()** 会返回错误。这个 port 自己拥有总线；共享 SPI 总线需要额外改造。40MHz 在长线或杜邦线上可能不稳定，先降到 20MHz 或 10MHz 验证。

### 3.5 在 main.c 中使用

```c
#include <string.h>

#include "esp_log.h"
#include "w25q80.h"

void app_main(void)
{
    w25q80_info_t info;
    uint8_t wr[16] = {0};
    uint8_t rd[16] = {0};
    uint32_t i;
    int ret;

    ret = w25q80_init(NULL);
    if (ret != 0)
    {
        ESP_LOGE("APP", "w25q80_init failed: %d", ret);
        return;
    }

    ret = w25q80_read_jedec_id(&info);
    if (ret != 0)
    {
        ESP_LOGE("APP", "read JEDEC failed: %d", ret);
        w25q80_deinit();
        return;
    }

    ESP_LOGI("APP", "JEDEC: %02X %02X %02X",
             info.manufacturer_id, info.memory_type, info.capacity);

    for (i = 0; i < (uint32_t)sizeof(wr); i++)
    {
        wr[i] = (uint8_t)i;
    }

    ret = w25q80_sector_erase(0x000000);
    if (ret != 0)
    {
        ESP_LOGE("APP", "sector erase failed: %d", ret);
        w25q80_deinit();
        return;
    }

    ret = w25q80_write(0x000000, wr, (uint32_t)sizeof(wr));
    if (ret != 0)
    {
        ESP_LOGE("APP", "write failed: %d", ret);
        w25q80_deinit();
        return;
    }

    ret = w25q80_read(0x000000, rd, (uint32_t)sizeof(rd));
    if (ret != 0)
    {
        ESP_LOGE("APP", "read failed: %d", ret);
        w25q80_deinit();
        return;
    }

    if (memcmp(wr, rd, sizeof(wr)) == 0)
    {
        ESP_LOGI("APP", "read/write OK");
    }
    else
    {
        ESP_LOGE("APP", "read/write mismatch");
    }

    w25q80_deinit();
}
```

要点：

- 应用只包含 **w25q80.h**，不要包含 **w25q80_port.h**。
- 应用不调用 **w25q80_port_get_driver()**；**w25q80_init()** 内部会调用它。
- W25Q80 是 NOR Flash，写之前必须先擦除。示例先擦 4KB 扇区，再写 16 字节。
- 示例使用地址 **0x000000**，会擦除第一个 4KB 扇区。确认这块外部 Flash 没有存你需要保留的数据。
- 读取大块数据时注意 port 的 **max_transfer_sz = 4092**，大块读取可能需要分段。

正常日志大致是：

```text
I (xxx) APP: JEDEC: EF 13 14
I (xxx) APP: read/write OK
```

### 3.6 重新配置、构建、烧录

修改 **DEMO_MODULES**、manifest、**PRIV_REQUIRES**、**target_compile_definitions** 后，CMake 必须重新 configure：

```powershell
idf.py -C demo/esp32s3_demo reconfigure
```

然后构建：

```powershell
.\demo.ps1 -Demo esp32s3_demo -Action build
```

烧录并监视：

```powershell
.\demo.ps1 -Demo esp32s3_demo -Action flash-monitor -Port COM5
```

如果 CMake 缓存出现奇怪问题：

```powershell
.\demo.ps1 -Demo esp32s3_demo -Action fullclean
.\demo.ps1 -Demo esp32s3_demo -Action build
```

### 3.7 确认 CMake 真的展开了模块

**demo_modules.cmake** 最后会打印一行：

```text
demo: 平台=esp32 模块=w25q80
```

如果模块列表是空的，说明 **DEMO_MODULES** 没在 include 之前设置。

还可以临时加调试输出：

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../common/demo_modules.cmake")

message(STATUS "DEMO_SRCS=${DEMO_SRCS}")
message(STATUS "DEMO_INC_DIRS=${DEMO_INC_DIRS}")
```

重新 configure 后应该能看到 w25q80 的 core 和 port 路径。确认完可以删掉这两行。

也可以查看 **build/compile_commands.json**，搜索 **w25q80.c** 和 **esp32_w25q80_port.c**，能看到它们的编译命令和 include 路径。

---

## 4. demo_modules.cmake 内部做了什么

核心逻辑可以简化成：

```cmake
set(DEMO_MODULE_QUEUE ${DEMO_MODULES})
set(DEMO_SRCS "")
set(DEMO_INC_DIRS "")

while(DEMO_MODULE_QUEUE)
    list(GET DEMO_MODULE_QUEUE 0 _demo_module)
    list(REMOVE_AT DEMO_MODULE_QUEUE 0)

    if(NOT "${_demo_module}" IN_LIST DEMO_MODULE_SEEN)
        set(_demo_manifest "${DEMO_COMMON_DIR}/manifests/${_demo_module}.cmake")
        if(NOT EXISTS "${_demo_manifest}")
            message(FATAL_ERROR "未找到模块清单: ${_demo_manifest}")
        endif()

        set(DEMO_MODULE_FILES "")
        set(DEMO_MODULE_INCLUDES "")
        set(DEMO_MODULE_DEPENDS "")

        include("${_demo_manifest}")

        list(APPEND DEMO_SRCS ${DEMO_MODULE_FILES})
        list(APPEND DEMO_INC_DIRS ${DEMO_MODULE_INCLUDES})
        list(APPEND DEMO_MODULE_SEEN ${_demo_module})
        list(APPEND DEMO_MODULE_QUEUE ${DEMO_MODULE_DEPENDS})
    endif()
endwhile()

list(REMOVE_DUPLICATES DEMO_SRCS)
list(REMOVE_DUPLICATES DEMO_INC_DIRS)
```

要记住的只有几点：

- 模块名决定清单文件名：**foo** 对应 **manifests/foo.cmake**。
- 清单里的路径优先用 **${DEMO_MODULE_ROOT}** 绝对路径。
- 每次 include 清单前，三个 **DEMO_MODULE_*** 变量都会被清空，所以清单要自己完整设置。
- **DEMO_MODULE_DEPENDS** 里的模块会自动进入队列。
- 这个文件只负责展开变量，不负责编译和链接。

---

## 5. CMake 变量对照

- **DEMO_PLATFORM**：由 **main/CMakeLists.txt** 设置，由 **demo_modules.cmake** 和 manifest 读取。作用：选择平台 port，例如 **esp32** 或 **stm32f1**。
- **DEMO_MODULES**：由 **main/CMakeLists.txt** 设置，由 **demo_modules.cmake** 读取。作用：本 demo 要用的顶层模块列表。
- **DEMO_MODULE_FILES**：由 manifest 设置，由 **demo_modules.cmake** 读取。作用：模块要编译的 .c 文件。
- **DEMO_MODULE_INCLUDES**：由 manifest 设置，由 **demo_modules.cmake** 读取。作用：模块头文件目录。
- **DEMO_MODULE_DEPENDS**：由 manifest 设置，由 **demo_modules.cmake** 读取。作用：仓库内其他模块依赖。
- **DEMO_SRCS**：由 **demo_modules.cmake** 生成，由 **main/CMakeLists.txt** 读取。作用：汇总后的所有源文件，传给 **idf_component_register(SRCS ...)**。
- **DEMO_INC_DIRS**：由 **demo_modules.cmake** 生成，由 **main/CMakeLists.txt** 读取。作用：汇总后的所有 include 路径，传给 **target_include_directories**。
- **PRIV_REQUIRES**：由 **main/CMakeLists.txt** 设置，由 ESP-IDF 组件管理器读取。作用：ESP-IDF 组件依赖，例如 **driver**、**esp_timer**。

---

## 6. 两类依赖不要混

- ESP-IDF 组件依赖：写 **PRIV_REQUIRES**，例如 **driver**、**esp_timer**、**esp_rom**、**freertos**、**nvs_flash**。
- 仓库内模块依赖：写 manifest 的 **DEMO_MODULE_DEPENDS**，例如 **eeprom** 依赖 **swi2c**。

不要把 **driver** 写进 **DEMO_MODULE_DEPENDS**，否则 CMake 会去找 **manifests/driver.cmake**。
也不要把 **utility** 写进 **PRIV_REQUIRES**，否则 IDF 会去找一个叫 **utility** 的 ESP-IDF 组件。

如果 **eeprom** 依赖 **swi2c**，**eeprom.cmake** 写：

```cmake
set(DEMO_MODULE_DEPENDS swi2c)
```

然后 **DEMO_MODULES** 只需要写 **eeprom**。**swi2c** 会被自动追加到队列。

---

## 7. 从 CMake 到链接

```text
demo/esp32s3_demo/CMakeLists.txt
  └─ include($ENV{IDF_PATH}/tools/cmake/project.cmake)
  └─ project(esp32s3_demo)
        └─ ESP-IDF 组件扫描
              └─ main/CMakeLists.txt
                    └─ include(../common/demo_modules.cmake)
                          └─ include(manifests/<module>.cmake)
                    └─ idf_component_register(SRCS main.c ${DEMO_SRCS})
                    └─ target_include_directories(${COMPONENT_LIB} PRIVATE ${DEMO_INC_DIRS})
        └─ 生成组件库 libmain.a / libdriver.a / ...
        └─ 最终链接 esp32s3_demo.elf
```

以 **w25q80** 为例，链接时的符号链是：

```text
app_main (main.o)
  └─ w25q80_init (w25q80.o)
       └─ w25q80_port_get_driver (esp32_w25q80_port.o)
            └─ spi_bus_initialize / spi_device_polling_transmit / gpio_set_level
               (libdriver.a)
```

模块源码会编进 **build/esp-idf/main/libmain.a**。**main.c** 调用 **w25q80_init()** 后，链接器才会把 **w25q80.o** 拉出来；**w25q80.o** 再要求 **w25q80_port_get_driver()**，再拉出 **esp32_w25q80_port.o** 和 **libdriver.a**。

ESP-IDF 默认使用 **-ffunction-sections** 和 **--gc-sections**。如果 **main.c** 完全没有引用某个模块的符号，链接器可能不会把该模块的对象拉进最终镜像。模块自身的语法错误仍会在编译阶段报出来，但它的 port 缺失这类链接错误可能暂时不暴露。所以真正验证一个模块，至少要在 **main.c** 里调用一个公开 API。

---

## 8. 常见错误

- 把模块名加进 **demo_modules.cmake**：不需要。模块名写 **DEMO_MODULES**，清单写 **manifests/<模块>.cmake**。
- **未找到模块清单: .../manifests/foo.cmake**：**DEMO_MODULES** 里有 **foo**，但清单不存在或文件名拼错。
- **foo: 未适配平台 esp32**：manifest 里没有 **if(DEMO_PLATFORM STREQUAL "esp32")** 分支。
- **foo.h: No such file or directory**：**DEMO_MODULE_INCLUDES** 少了 **include/**。
- **foo_port.h: No such file or directory**：**DEMO_MODULE_INCLUDES** 少了 **port/**。
- **undefined reference to 'foo_init'**：**DEMO_MODULE_FILES** 少了 **core/foo.c**。
- **undefined reference to 'foo_port_get_driver'**：**DEMO_MODULE_FILES** 少了 **port/esp32_foo_port.c**，或平台分支没进。
- **driver/spi_master.h: No such file or directory**：**PRIV_REQUIRES** 少了 **driver**。
- **target_include_directories 报 ${COMPONENT_LIB} 为空**：它放在了 **idf_component_register** 之前。
- 改了 **DEMO_MODULES** 但构建没变化：CMake 没重新 configure，执行 **idf.py -C demo/esp32s3_demo reconfigure**。
- 模块编译了但没有任何效果：**main.c** 没有调用模块 API，链接器可能没有把它的对象拉进最终镜像。

---

## 9. 新增模块检查清单

- [ ] **modules/<模块>/include/<模块>.h** 存在，是公共 API。
- [ ] **modules/<模块>/core/*.c** 存在。
- [ ] **modules/<模块>/port/esp32_*.c** 存在，并且硬件宏与实际接线一致。
- [ ] **demo/esp32s3_demo/common/manifests/<模块>.cmake** 存在。
- [ ] **DEMO_MODULE_FILES** 列出了 core 和 ESP32 port 的 .c。
- [ ] **DEMO_MODULE_INCLUDES** 列出了 **include/** 和 **port/**。
- [ ] 仓库内依赖写进了 **DEMO_MODULE_DEPENDS**。
- [ ] **DEMO_MODULES** 写入了模块名。
- [ ] ESP-IDF 组件依赖写进了 **PRIV_REQUIRES**。
- [ ] **main.c** 包含公共头并调用了至少一个公开 API。
- [ ] 执行过 **idf.py -C demo/esp32s3_demo reconfigure**。
- [ ] 构建、烧录、监视正常。

---

## 10. 通用清单模板

```cmake
set(DEMO_MODULE_FILES
    "${DEMO_MODULE_ROOT}/<module>/core/<module>.c"
)

if(DEMO_PLATFORM STREQUAL "esp32")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/<module>/port/esp32_<module>_port.c"
    )
elseif(DEMO_PLATFORM STREQUAL "stm32f1")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/<module>/port/stm32_<module>_port.c"
    )
else()
    message(FATAL_ERROR "<module>: 未适配平台 ${DEMO_PLATFORM}")
endif()

set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/<module>/include"
    "${DEMO_MODULE_ROOT}/<module>/port"
)

set(DEMO_MODULE_DEPENDS "")
```

如果模块有多个核心文件，例如 **ws2812** 的 **ws2812.c**、**ws2812_color.c**、**ws2812_effect.c**，把它们全部列进 **DEMO_MODULE_FILES**。

如果模块只有头文件，例如 **port demo**，**DEMO_MODULE_FILES** 可以为空，只列 **DEMO_MODULE_INCLUDES**。这种模块只能提供声明，调用它的 getter 仍然会链接失败，除非另有实现。

---

## 11. 一句话总结

**demo_modules.cmake** 是通用展开器，不要往里面加模块名；模块名写在 **main/CMakeLists.txt** 的 **DEMO_MODULES**，模块文件、头文件、仓库内依赖写在 **demo/esp32s3_demo/common/manifests/<模块>.cmake**。真正把源码交给 ESP-IDF 的是 **idf_component_register(SRCS ... ${DEMO_SRCS})**，真正让头文件可见的是 **target_include_directories(... ${DEMO_INC_DIRS})**。
