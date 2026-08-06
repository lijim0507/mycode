


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