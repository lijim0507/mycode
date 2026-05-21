/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "swi2c.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#define I2C_GPIO_PORT GPIOB
#define I2C_SDA_PIN GPIO_PIN_7
#define I2C_SCL_PIN GPIO_PIN_6

#define SCL_OUTPUT(x) HAL_GPIO_WritePin(I2C_GPIO_PORT, I2C_SCL_PIN, (GPIO_PinState)x)
#define SDA_OUTPUT(x) HAL_GPIO_WritePin(I2C_GPIO_PORT, I2C_SDA_PIN, (GPIO_PinState)x)

#define SDA_INPUT() HAL_GPIO_ReadPin(I2C_GPIO_PORT, I2C_SDA_PIN)

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
void swi2c_init(void)
{

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*the i2c gpio port should be set OD mode*/
    GPIO_InitStruct.Pin = I2C_SDA_PIN | I2C_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; /*IO Pull down only, cannot pull up, need external pull up resist*/
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(I2C_GPIO_PORT, &GPIO_InitStruct);

    SCL_OUTPUT(1); //
    SDA_OUTPUT(1); //
}
/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
static void swi2c_delay()
{
    for (uint8_t i = 0; i < 50; i++)
    {
        __nop();
    }
}
/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
void swi2c_start(void)
{
    /*    _____
     *SDA      \_____________
     *    __________
     *SCL           \________
     */

    // release the bus
    SDA_OUTPUT(1);
    SCL_OUTPUT(1);
    swi2c_delay();
    SDA_OUTPUT(0);
    swi2c_delay();
    SCL_OUTPUT(0);
}
/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
void swi2c_stop(void)
{

    /*              _______
     *SDA __________/
     *          ____________
     *SCL _____/
     */

    SCL_OUTPUT(0);
    SDA_OUTPUT(0);
    swi2c_delay();
    SCL_OUTPUT(1);
    swi2c_delay();
    SDA_OUTPUT(1);
    SCL_OUTPUT(1);
}
/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
static void swi2c_send_bit(uint8_t bit)
{
    /*        ________
     *SDA ____X________X______
     *           _____
     *SCL ______/     \_____
     */

    /*pull low when data is going to change*/
    SCL_OUTPUT(0);
    if (bit > 0)
    {
        SDA_OUTPUT(1);
    }
    else
    {
        SDA_OUTPUT(0);
    }
    swi2c_delay();
    /*pull up when data is ready to be read*/
    SCL_OUTPUT(1);
    swi2c_delay();
    /*after data to be read, release the bus*/
    SCL_OUTPUT(0);
    SDA_OUTPUT(0);
}

/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
static uint8_t swi2c_receive_bit(void)
{
    /*        ________
     *SDA ____X________X______
     *           _____
     *SCL ______/     \_____
     */
    uint8_t flag = 0;
    /*release the bus*/
    SDA_OUTPUT(1);
    SCL_OUTPUT(0);
    /*read bus data*/
    swi2c_delay();
    SCL_OUTPUT(1);
    flag = SDA_INPUT();
    /*release the bus*/
    SCL_OUTPUT(0);

    return flag;
}
/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
void swi2c_send_ask(uint8_t bit)
{

    swi2c_send_bit(bit);
}
/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
uint8_t swi2c_receive_ask(void)
{
    /*           ____
     *SCL ______/    \______
     *    __________________
     *SDA
     */

    uint8_t res;

    SDA_OUTPUT(1); // �����ͷ�SDA���ߣ��ôӻ�ռ��
    SCL_OUTPUT(0); //
    swi2c_delay();
    SCL_OUTPUT(1);
    swi2c_delay();
    res = SDA_INPUT(); // ��ȡ�ӻ�������ACK(0)����NACK(1)�ź�
    SCL_OUTPUT(0);
    return res;
}

/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
void swi2c_send_byte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        swi2c_send_bit(byte & (0x80 >> i));
    }
}

/**************************************************************************
 * @brief
 *
 * @param
 * @param
 *
 * @return none
 *
 * @ Pass/ Fail criteria: none
 ******************************************************************************/
uint8_t swi2c_receive_byte(void)
{
    uint8_t byte = 0;
    uint8_t temp = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        temp = swi2c_receive_bit();
        byte += (temp << (7 - i));
    }

    return byte;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
