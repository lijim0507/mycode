/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "udisk.h"
#include "udisk_port.h"

#include "utilities.h"
#include <string.h>
#include <stdio.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define NUMBER_FILE_NAME      "config.dat"
#define FILENAME_MAX_SIZE     32
#define PI                    3.14159265358979323846f

#define UDISK_MOTOR_POSITION_MIN   -PI
#define UDISK_MOTOR_POSITION_MAX   PI
#define UDISK_MOTOR_SPEED_MIN      -33.0f
#define UDISK_MOTOR_SPEED_MAX      33.0f
#define UDISK_MOTOR_TORQUE_MIN     -17.0f
#define UDISK_MOTOR_TORQUE_MAX     17.0f
#define UDISK_MOTOR_IQ_MIN         -50.0f
#define UDISK_MOTOR_IQ_MAX         50.0f
#define UDISK_GYRO_MIN             -500.0f
#define UDISK_GYRO_MAX             500.0f
#define UDISK_ACCEL_MIN            -4.0f
#define UDISK_ACCEL_MAX            4.0f

#define UDISK_SYNC_COUNT           100
#define LABEL_UPDATE_TIMEOUT       5000

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct {
    char chip_id[4];
    char serial_number[4];
    char mode[4];
    char lable[4];
    char end_character[1];
} data_file_name_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void     store_data_pack(void);
static uint16_t get_serial_number(void);
static void     reset_data_file_name(void);
static void     update_data_file_name(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const udisk_driver_t *g_driver;
static bool                  g_initialized;

static udisk_file_t g_file;
static udisk_storage_data_t g_storage_data;

static data_file_name_t g_file_name = {
    .chip_id        = "0000",
    .serial_number  = "0000",
    .mode           = "0000",
    .lable          = "0000",
    .end_character  = {'\0'},
};
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int udisk_init(const udisk_driver_t *driver)
{
    if (!driver || !driver->init || !driver->file_open) {
        return -1;
    }

    if (g_initialized) {
        udisk_deinit();
    }

    if (driver->init() != UDISK_OK) {
        return -2;
    }

    g_driver = driver;

    uint32_t file_number = 0;
    udisk_file_t num_file = NULL;

    if (g_driver->file_open(&num_file, NUMBER_FILE_NAME, 0x0C) == UDISK_OK) {
        g_driver->file_read(num_file, &file_number, sizeof(file_number));
        g_driver->file_close(num_file);
    }

    g_initialized = true;
    return 0;
}

int udisk_deinit(void)
{
    if (!g_initialized) {
        return 0;
    }

    if (g_driver && g_driver->deinit) {
        g_driver->deinit();
    }

    g_driver      = NULL;
    g_file        = NULL;
    g_initialized = false;
    return 0;
}

void udisk_task(void *param)
{
    (void)param;
    uint32_t write_count = 0;

    for (;;) {
        store_data_pack();

        if (g_driver) {
            g_driver->delay_ms(1);
        }

        if (g_driver->host_status() != UDISK_OK) {
            udisk_init(g_driver);
            continue;
        }

        if (g_driver->is_file_open(g_file)) {
            if (g_driver->file_write(g_file, &g_storage_data,
                                      sizeof(udisk_storage_data_t)) == UDISK_OK) {
                write_count++;
            }

            if (write_count >= UDISK_SYNC_COUNT) {
                if (g_driver->file_sync(g_file) == UDISK_OK) {
                    write_count = 0;
                }
            }
        }
    }
}

void udisk_create_data_file(void)
{
    char filename[FILENAME_MAX_SIZE] = {0};
    uint16_t serial_num;

    if (g_driver->is_file_open(g_file)) {
        g_driver->file_close(g_file);
    }

    reset_data_file_name();

    snprintf(g_file_name.chip_id, sizeof(g_file_name.chip_id), "1234");

    serial_num = get_serial_number();
    snprintf(g_file_name.serial_number, sizeof(g_file_name.serial_number),
             "%04d", serial_num);

    snprintf(filename, FILENAME_MAX_SIZE, "%s_%s_%s_%s%s.dat",
             g_file_name.chip_id, g_file_name.serial_number,
             g_file_name.mode, g_file_name.lable,
             g_file_name.end_character);

    g_driver->file_open(&g_file, filename, 0x0C);
}

void udisk_end_data_file(void)
{
    if (!g_driver->is_file_open(g_file)) {
        return;
    }

    update_data_file_name();

    g_driver->file_sync(g_file);
    g_driver->file_close(g_file);

    reset_data_file_name();
}

void udisk_set_file_label(uint8_t label)
{
    static char label_str[4] = {0};
    static uint8_t label_index = 0;
    static uint32_t last_update_time = 0;

    if (last_update_time == 0 ||
        strcmp(label_str, "0000") == 0 ||
        (g_driver->get_time() - last_update_time) >= LABEL_UPDATE_TIMEOUT) {
        label_str[0] = '0';
        label_str[1] = '0';
        label_str[2] = '0';
        label_str[3] = '0';
        label_index = 0;
    }

    label_str[label_index++] = (char)(label + '0');
    snprintf(g_file_name.lable, sizeof(g_file_name.lable), "%s", label_str);

    last_update_time = g_driver->get_time();
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static uint16_t get_serial_number(void)
{
    uint32_t file_number = 0;
    udisk_file_t num_file = NULL;

    if (g_driver->file_open(&num_file, NUMBER_FILE_NAME, 0x02) == UDISK_OK) {
        g_driver->file_read(num_file, &file_number, sizeof(file_number));
        file_number++;
        g_driver->file_write(num_file, &file_number, sizeof(file_number));
        g_driver->file_sync(num_file);
        g_driver->file_close(num_file);
        return (uint16_t)(file_number - 1);
    }

    return 0;
}

static void reset_data_file_name(void)
{
    snprintf(g_file_name.chip_id, sizeof(g_file_name.chip_id), "0000");
    snprintf(g_file_name.serial_number, sizeof(g_file_name.serial_number), "0000");
    snprintf(g_file_name.mode, sizeof(g_file_name.mode), "0000");
    snprintf(g_file_name.lable, sizeof(g_file_name.lable), "0000");
}

static void update_data_file_name(void)
{
    (void)(void);
}

static void store_data_pack(void)
{
    /* placeholder: cross-module data calls preserved as-is */
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
