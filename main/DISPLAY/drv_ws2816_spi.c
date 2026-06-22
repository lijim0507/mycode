#include "drv_ws2816_spi.h"

#include "main.h"
#include <stddef.h>

/* SPI1 at 3 MHz: 0 = 1000 (333/1000 ns), 1 = 1100 (667/667 ns). */
#define WS2816_SPI_CODE_0  0x08U
#define WS2816_SPI_CODE_1  0x0CU

static void spi_write_ws2816_byte(uint8_t data)
{
    for (int8_t bit = 7; bit > 0; bit -= 2)
    {
        uint8_t encoded = (data & (1U << (uint8_t)bit))
                        ? (uint8_t)(WS2816_SPI_CODE_1 << 4)
                        : (uint8_t)(WS2816_SPI_CODE_0 << 4);

        encoded |= (data & (1U << (uint8_t)(bit - 1)))
                 ? WS2816_SPI_CODE_1 : WS2816_SPI_CODE_0;

        while ((SPI1->SR & SPI_SR_TXE) == 0U)
        {
        }
        *(__IO uint8_t *)&SPI1->DR = encoded;
    }
}

static int ws2816_spi_init(void)
{
    // GPIO_InitTypeDef gpio = {0};

    // __HAL_RCC_GPIOA_CLK_ENABLE();
    // __HAL_RCC_SPI1_CLK_ENABLE();

    // gpio.Pin = WS2816_DATA_Pin;
    // gpio.Mode = GPIO_MODE_AF_PP;
    // gpio.Pull = GPIO_PULLDOWN;
    // gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    // gpio.Alternate = GPIO_AF0_SPI1;
    // HAL_GPIO_Init(WS2816_DATA_GPIO_Port, &gpio);

    // SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_1 | SPI_CR1_BR_0
    //           | SPI_CR1_SSM | SPI_CR1_SSI
    //           | SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
    // SPI1->CR2 = SPI_CR2_DS_0 | SPI_CR2_DS_1 | SPI_CR2_DS_2 | SPI_CR2_FRXTH;
    // SPI1->CR1 |= SPI_CR1_SPE;

    // HAL_Delay(1U);
    // return 0;
}

static int ws2816_spi_transmit(const ws2816_value_t *pixels,
                               uint32_t led_count,
                               uint8_t brightness,
                               const ws2816_gain_t *gain)
{
    uint32_t primask;
    uint16_t gain_header;

    if (pixels == NULL || gain == NULL || led_count == 0U)
    {
        return -1;
    }

    /* Interrupt latency would extend the low time between encoded bytes. */
    primask = __get_PRIMASK();
    __disable_irq();

    /* Gain header: GGGGGRRR RRBBBBB0, MSB first. */
    gain_header = ((uint16_t)(gain->green & WS2816_CURRENT_GAIN_MAX) << 11)
                | ((uint16_t)(gain->red & WS2816_CURRENT_GAIN_MAX) << 6)
                | ((uint16_t)(gain->blue & WS2816_CURRENT_GAIN_MAX) << 1);
    spi_write_ws2816_byte((uint8_t)(gain_header >> 8));
    spi_write_ws2816_byte((uint8_t)gain_header);

    for (uint32_t i = 0; i < led_count * WS2816_CHANNELS_PER_LED; i++)
    {
        ws2816_value_t value = pixels[i];

        if (brightness != 255U)
        {
            value = (ws2816_value_t)(((uint32_t)value * brightness) / 255U);
        }
        spi_write_ws2816_byte((uint8_t)(value >> 8));
        spi_write_ws2816_byte((uint8_t)value);
    }

    while ((SPI1->SR & SPI_SR_TXE) == 0U)
    {
    }
    while ((SPI1->SR & SPI_SR_BSY) != 0U)
    {
    }

    if (primask == 0U)
    {
        __enable_irq();
    }

    HAL_Delay(1U); /* WS2816A reset/latch requires at least 280 us low. */
    return 0;
}

static int ws2816_spi_deinit(void)
{
    // GPIO_InitTypeDef gpio = {0};

    // while ((SPI1->SR & SPI_SR_BSY) != 0U)
    // {
    // }
    // SPI1->CR1 &= ~SPI_CR1_SPE;
    // __HAL_RCC_SPI1_CLK_DISABLE();

    // gpio.Pin = WS2816_DATA_Pin;
    // gpio.Mode = GPIO_MODE_OUTPUT_PP;
    // gpio.Pull = GPIO_PULLDOWN;
    // gpio.Speed = GPIO_SPEED_FREQ_LOW;
    // HAL_GPIO_Init(WS2816_DATA_GPIO_Port, &gpio);
    // HAL_GPIO_WritePin(WS2816_DATA_GPIO_Port, WS2816_DATA_Pin, GPIO_PIN_RESET);
    // return 0;
}

const ws2816_driver_t *ws2816_spi_driver_get(void)
{
    static const ws2816_driver_t driver = {
        .init = ws2816_spi_init,
        .transmit = ws2816_spi_transmit,
        .deinit = ws2816_spi_deinit,
    };
    return &driver;
}
