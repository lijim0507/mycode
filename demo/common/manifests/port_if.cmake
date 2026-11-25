# 模块清单：port demo（仅头文件）
# at / eeprom / imu / odrive_can 需要这里的类型定义；
# port demo 下的 .c 是空桩，不能参与编译。

set(DEMO_MODULE_FILES "")
set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/port demo"
)
set(DEMO_MODULE_DEPENDS "")

