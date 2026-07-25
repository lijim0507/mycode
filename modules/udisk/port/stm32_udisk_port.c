/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "udisk_port.h"

#include "ff.h"
#include "fatfs.h"
#include "usb_host.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define UDISK_INIT_TIMEOUT_MS 5000
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int      stm32_udisk_init(void);
static int      stm32_udisk_deinit(void);
static int      stm32_udisk_host_status(void);
static int      stm32_udisk_mount(void);
static int      stm32_udisk_file_open(udisk_file_t *file, const char *name, uint8_t mode);
static int      stm32_udisk_file_close(udisk_file_t file);
static int      stm32_udisk_file_write(udisk_file_t file, const void *data, uint32_t size);
static int      stm32_udisk_file_read(udisk_file_t file, void *data, uint32_t size);
static int      stm32_udisk_file_sync(udisk_file_t file);
static int      stm32_udisk_file_rename(const char *old_path, const char *new_path);
static int      stm32_udisk_is_file_open(udisk_file_t file);
static uint32_t stm32_udisk_get_time(void);
static void     stm32_udisk_delay(uint32_t ms);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
extern volatile uint32_t Appli_state;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const udisk_driver_t *udisk_port_get_driver(void)
{
    static const udisk_driver_t driver = {
        .init         = stm32_udisk_init,
        .deinit       = stm32_udisk_deinit,
        .host_status  = stm32_udisk_host_status,
        .mount        = stm32_udisk_mount,
        .file_open    = stm32_udisk_file_open,
        .file_close   = stm32_udisk_file_close,
        .file_write   = stm32_udisk_file_write,
        .file_read    = stm32_udisk_file_read,
        .file_sync    = stm32_udisk_file_sync,
        .file_rename  = stm32_udisk_file_rename,
        .is_file_open = stm32_udisk_is_file_open,
        .get_time     = stm32_udisk_get_time,
        .delay_ms     = stm32_udisk_delay,
    };
    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int stm32_udisk_init(void)
{
    uint32_t start_tick = stm32_udisk_get_time();

    if (stm32_udisk_host_status() != UDISK_OK) {
        return UDISK_ERROR;
    }

    while (stm32_udisk_host_status() != UDISK_OK) {
        uint32_t elapsed = stm32_udisk_get_time() - start_tick;
        if (elapsed >= UDISK_INIT_TIMEOUT_MS) {
            return UDISK_ERROR;
        }
        stm32_udisk_delay(10);
    }

    if (f_mount(&USBHFatFS, USBHPath, 1) != FR_OK) {
        return UDISK_ERROR;
    }

    return UDISK_OK;
}

static int stm32_udisk_deinit(void)
{
    return UDISK_OK;
}

static int stm32_udisk_host_status(void)
{
    return (Appli_state == 0x02) ? UDISK_OK : UDISK_ERROR;
}

static int stm32_udisk_mount(void)
{
    return (f_mount(&USBHFatFS, USBHPath, 1) == FR_OK) ? UDISK_OK : UDISK_ERROR;
}

static int stm32_udisk_file_open(udisk_file_t *file, const char *name, uint8_t mode)
{
    if (!name) {
        return UDISK_ERROR;
    }
    return (f_open((FIL *)file, name, mode) == FR_OK) ? UDISK_OK : UDISK_ERROR;
}

static int stm32_udisk_file_close(udisk_file_t file)
{
    return (f_close((FIL *)file) == FR_OK) ? UDISK_OK : UDISK_ERROR;
}

static int stm32_udisk_file_write(udisk_file_t file, const void *data, uint32_t size)
{
    UINT written = 0;
    if (!data || size == 0) {
        return UDISK_ERROR;
    }
    return (f_write((FIL *)file, data, size, &written) == FR_OK && written == size)
           ? UDISK_OK : UDISK_ERROR;
}

static int stm32_udisk_file_read(udisk_file_t file, void *data, uint32_t size)
{
    UINT read_count = 0;
    if (!data || size == 0) {
        return UDISK_ERROR;
    }
    return (f_read((FIL *)file, data, size, &read_count) == FR_OK && read_count == size)
           ? UDISK_OK : UDISK_ERROR;
}

static int stm32_udisk_file_sync(udisk_file_t file)
{
    return (f_sync((FIL *)file) == FR_OK) ? UDISK_OK : UDISK_ERROR;
}

static int stm32_udisk_file_rename(const char *old_path, const char *new_path)
{
    if (!old_path || !new_path) {
        return UDISK_ERROR;
    }
    return (f_rename(old_path, new_path) == FR_OK) ? UDISK_OK : UDISK_ERROR;
}

static int stm32_udisk_is_file_open(udisk_file_t file)
{
    return (file != NULL) ? 1 : 0;
}

static uint32_t stm32_udisk_get_time(void)
{
    return HAL_GetTick();
}

static void stm32_udisk_delay(uint32_t ms)
{
    HAL_Delay(ms);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
