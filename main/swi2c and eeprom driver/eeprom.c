/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "eeprom.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#if 0 /* Hardware I2C */

#include "i2c.h"
#define HardwareI2c

#else /* Software I2C */

#include "swi2c.h"
#define SoftwareI2c

#endif

#define EEPROM_PAGE_SIZE 16     /* Unit: Byte */
#define EEPROM_MEMORY_SIZE 2048 /* Unit: Byte */
#define EEPROM_PAGE_NUMBER (EEPROM_MEMORY_SIZE / EEPROM_PAGE_SIZE)
#define EEPROM_RW_TIMEOUT 3 /* Unit: ms */

#define EEPROM_CONTROL_COMMAND 0xA0
#define EEPROM_DEVICE_ADDRESS 0x00

#define EEPROM_W_BIT 0x00
#define EEPROM_R_BIT 0x01

#define EEPROM_W_ADDRESS (EEPROM_CONTROL_COMMAND + EEPROM_DEVICE_ADDRESS + EEPROM_W_BIT)
#define EEPROM_R_ADDRESS (EEPROM_CONTROL_COMMAND + EEPROM_DEVICE_ADDRESS + EEPROM_R_BIT)

#define EEPROM_WP_PORT GPIOB
#define EEPROM_WP_PIN GPIO_PIN_5

#define EEPROM_SET_WP(x)                                                    \
    do                                                                      \
    {                                                                       \
        HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, (GPIO_PinState)x); \
    } while (0)

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
/* Note: EEPROM communicates via I2C */

/****************************************************************************/
/*						Exported Functions									*/
/****************************************************************************/

/**
 * @brief  Initialize EEPROM (I2C interface)
 */
void eeprom_init(void)
{
#ifdef SoftwareI2c
    swi2c_init();
#endif

#ifdef HardwareI2c
    MX_I2C1_Init();
#endif
}

/**
 * @brief  Read bytes from EEPROM
 * @param  address     Start address to read from
 * @param  read_buf    Buffer to store read data
 * @param  read_size   Number of bytes to read
 * @note   EEPROM does not support auto page increment; page boundary must be handled manually
 */
void eeprom_read_bytes(uint8_t address, uint8_t *read_buf, uint8_t read_size)
{
    uint8_t temp1 = 0;
    uint8_t ack = 0;
    uint8_t temp_address = 0; /* Current read address */
    uint8_t page_num = 0;
    uint8_t page_counter = 0; /* Total pages to read, current page counter */

    /* Check if address is aligned to page start (lower 4 bits == 0) */
    page_num += ((address & 0x0F) == 0) ? 0 : 1; /* Start page: is it aligned to page start? */

    if (page_num == 0) /* Start address is at page start */
    {
        page_num += read_size / EEPROM_PAGE_SIZE; /* Full page count */
    }
    else /* Start address is not at page start */
    {
        page_num += (read_size - (EEPROM_PAGE_SIZE - (address & 0x0F))) / EEPROM_PAGE_SIZE; /* Full page count */
    }

    if ((read_size - (EEPROM_PAGE_SIZE - (address & 0x0F))) > 0)
    {
        page_num += ((read_size - (address & 0x0F)) % EEPROM_PAGE_SIZE == 0) ? 0 : 1; /* Remaining page count */
    }

    EEPROM_SET_WP(0);
    temp_address = address;

    /* EEPROM does not support page auto-increment; each page requires re-writing the address */
    for (page_counter = 0; page_counter < page_num; page_counter++)
    {
#ifdef SoftwareI2c
        /* Dummy Write: send device address + word address before reading */
        swi2c_start();

        /* Send device write address */
        swi2c_send_byte(EEPROM_W_ADDRESS);
        ack = swi2c_receive_ask();
        if (0 != ack)
        {
            /* Fail: EEPROM did not acknowledge */
            __nop();
        }

        /* Send word address */
        swi2c_send_byte(temp_address);
        ack = swi2c_receive_ask();
        if (0 != ack)
        {
            /* Fail: EEPROM did not acknowledge */
            __nop();
        }
        /* Dummy Write End */

        /* Repeated start for read */
        swi2c_start();

        /* Send device read address */
        swi2c_send_byte(EEPROM_R_ADDRESS);
        ack = swi2c_receive_ask();
        if (0 != ack)
        {
            /* Fail: EEPROM did not acknowledge */
            __nop();
        }

        /* Data Read */
        do
        {
            temp1 = swi2c_receive_byte();
            *(read_buf + temp_address - address) = temp1;
            temp_address++;

            /* Send NACK when reaching page boundary or end of requested range */
            if (((temp_address & 0x0F) == 0) || (temp_address >= address + read_size))
            {
                swi2c_send_ask(1); /* NACK */
            }
            else
            {
                swi2c_send_ask(0); /* ACK */
            }
        } while (((temp_address % EEPROM_PAGE_SIZE) != 0) && (temp_address < (address + read_size)));

        swi2c_stop();
        HAL_Delay(2);

#endif

#ifdef HardwareI2c
        temp1 = HAL_I2C_Mem_Read(&hi2c1, EEPROM_R_ADDRESS, address, I2C_MEMADD_SIZE_8BIT, read_buf, read_size, 0xFF);
#endif
    }

    EEPROM_SET_WP(1);
}

/**
 * @brief  Write bytes to EEPROM
 * @param  address     Start address to write to
 * @param  write_buf   Buffer containing data to write
 * @param  write_size  Number of bytes to write
 * @note   EEPROM does not support auto page increment; page boundary must be handled manually
 */
void eeprom_write_bytes(uint8_t address, uint8_t *write_buf, uint8_t write_size)
{
    uint8_t temp1 = 0;
    uint8_t ack = 0;
    uint8_t temp_address = 0;
    uint8_t page_counter = 0;
    uint8_t page_num = 0;

    /* Check if address is aligned to page start (lower 4 bits == 0) */
    page_num += ((address & 0x0F) == 0) ? 0 : 1; /* Start page: is it aligned to page start? */

    if (page_num == 0) /* Start address is at page start */
    {
        page_num += write_size / EEPROM_PAGE_SIZE; /* Full page count */
    }
    else /* Start address is not at page start */
    {
        page_num += (write_size - (EEPROM_PAGE_SIZE - (address & 0x0F))) / EEPROM_PAGE_SIZE; /* Full page count */
    }

    if ((write_size - (EEPROM_PAGE_SIZE - (address & 0x0F))) > 0)
    {
        page_num += ((write_size - (address & 0x0F)) % EEPROM_PAGE_SIZE == 0) ? 0 : 1; /* Remaining page count */
    }

    EEPROM_SET_WP(0);
    temp_address = address;

    /* Page Write Cycle */
    for (page_counter = 0; page_counter < page_num; page_counter++)
    {
#ifdef SoftwareI2c
        swi2c_start();

        /* Send device write address */
        swi2c_send_byte((uint8_t)EEPROM_W_ADDRESS);
        ack = swi2c_receive_ask();
        if (1 == ack)
        {
            __nop();
            /* Fail: EEPROM did not acknowledge */
        }

        /* Send word address */
        swi2c_send_byte(temp_address);
        ack = swi2c_receive_ask();
        if (1 == ack)
        {
            __nop();
            /* Fail: EEPROM did not acknowledge */
        }

        /* Data Write */
        do
        {
            temp1 = *(write_buf + temp_address - address);
            temp_address++;
            swi2c_send_byte(temp1);
            ack = swi2c_receive_ask();
            if (1 == ack)
            {
                __nop();
                /* Fail: EEPROM did not acknowledge */
            }
        } while (((temp_address % EEPROM_PAGE_SIZE) != 0) && (temp_address < (address + write_size)));

        swi2c_stop();
        HAL_Delay(2);

#endif

#ifdef HardwareI2c
        temp1 = HAL_I2C_Mem_Write(&hi2c1, EEPROM_W_ADDRESS, address, I2C_MEMADD_SIZE_8BIT, write_buf, write_size, 0xFF);
#endif
    }

    EEPROM_SET_WP(1);
}

/**
 * @brief  Main function for EEPROM testing (demo)
 */
void eeprom_main_function(void)
{
    static uint8_t array1[2] = {0x64, 0x24};
    static uint8_t array2[2] = {0};

    // eeprom_write_bytes(0x1F, array1, 2);
    HAL_Delay(50);
    eeprom_read_bytes(0x1F, array2, 2);
    HAL_Delay(50);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
