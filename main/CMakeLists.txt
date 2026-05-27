file(GLOB_RECURSE srcs  "main.c" 
                        "uds/*.c"  
                        "can_simple/*.c")

set(ws2812_core_srcs
    "ws2812/core/ws2812.c"
    "ws2812/core/ws2812_color.c"
    "ws2812/core/ws2812_effect.c"
)

set(ws2812_port_srcs "ws2812/port/esp32_rmt.c")

set(swi2c_core_srcs
    "swi2c/core/swi2c.c"
)

set(swi2c_port_srcs "swi2c/port/swi2c_port.c")

set(eeprom_core_srcs
    "eeprom/core/eeprom.c"
)

set(key_core_srcs
    "key/core/key.c"
    "key/core/key_filter.c"
    "key/core/key_state.c"
    "key/core/key_event.c"
)

list(APPEND srcs ${ws2812_core_srcs} ${ws2812_port_srcs}
                 ${swi2c_core_srcs} ${swi2c_port_srcs}
                 ${eeprom_core_srcs}
                 ${key_core_srcs})

idf_component_register( SRCS "${srcs}"

                        INCLUDE_DIRS    "./uds"
                                        "./can_simple"
                                        "./ws2812/include"
                                        "./ws2812/port"
                                        "./swi2c/include"
                                        "./swi2c/port"
                                        "./eeprom/include"
                                        "./key/include"

                        PRIV_REQUIRES   spi_flash
                                        esp_driver_twai 
                                        esp_driver_uart
                                        esp_driver_spi
                                        esp_driver_gpio
                                        esp_driver_rmt)
