file(GLOB_RECURSE srcs  "main.c" 
                        "uds/*.c"  
                        "can_simple/*.c")

idf_component_register( SRCS "${srcs}"

                        INCLUDE_DIRS    "./uds"
                                        "./can_simple"

                        PRIV_REQUIRES   spi_flash
                                        esp_driver_twai 
                                        esp_driver_uart
                                        esp_driver_spi
                                        esp_driver_gpio)
