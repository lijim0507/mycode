file(GLOB_RECURSE srcs  "main.c" 
                        "uds/*.c"  
                        "can_simple/*.c")

set(ws2812_core_srcs
    "ws2812/core/ws2812.c"
    "ws2812/core/ws2812_color.c"
    "ws2812/core/ws2812_effect.c"
)

# Platform-specific port: only compile the port for this target
set(ws2812_port_srcs "ws2812/port/esp32_rmt.c")

list(APPEND srcs ${ws2812_core_srcs} ${ws2812_port_srcs})

idf_component_register( SRCS "${srcs}"

                        INCLUDE_DIRS    "./uds"
                                        "./can_simple"
                                        "./ws2812/include"
                                        "./ws2812/port"

                        PRIV_REQUIRES   spi_flash
                                        esp_driver_twai 
                                        esp_driver_uart
                                        esp_driver_spi
                                        esp_driver_gpio
                                        esp_driver_rmt)
