/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "board.h"

#include "imu.h"
#include "imu_port.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/
#define BOARD_IMU_SPI_HOST           SPI2_HOST
#define BOARD_IMU_MISO_PIN           GPIO_NUM_12
#define BOARD_IMU_MOSI_PIN           GPIO_NUM_13
#define BOARD_IMU_SCLK_PIN           GPIO_NUM_11
#define BOARD_IMU_CS_PIN             GPIO_NUM_15
#define BOARD_IMU_SPI_CLOCK_HZ       (10 * 1000 * 1000)
#define BOARD_IMU_SPI_MODE           0
#define BOARD_IMU_SPI_QUEUE_SIZE     7
#define BOARD_IMU_MAX_TRANSFER_SIZE  32

/****************************************************************************/
/*                          Exported Variables                              */
/****************************************************************************/
const imu_port_config_t g_board_imu_cfg = {
    .spi_host          = BOARD_IMU_SPI_HOST,
    .miso_pin          = BOARD_IMU_MISO_PIN,
    .mosi_pin          = BOARD_IMU_MOSI_PIN,
    .sclk_pin          = BOARD_IMU_SCLK_PIN,
    .cs_pin            = BOARD_IMU_CS_PIN,
    .clock_hz          = BOARD_IMU_SPI_CLOCK_HZ,
    .spi_mode          = BOARD_IMU_SPI_MODE,
    .queue_size        = BOARD_IMU_SPI_QUEUE_SIZE,
    .max_transfer_size = BOARD_IMU_MAX_TRANSFER_SIZE,
};

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/
int board_init(void)
{
    return board_imu_init();
}

int board_deinit(void)
{
    return board_imu_deinit();
}

int board_imu_init(void)
{
    if (imu_init(imu_port_get_driver(), (void *)&g_board_imu_cfg) != 0) {
        return BOARD_ERR_IMU;
    }

    return BOARD_OK;
}

int board_imu_deinit(void)
{
    imu_deinit();
    return BOARD_OK;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/
