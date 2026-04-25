# Appendix C: Pin Reference Tables

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## STM32F407VGT6 (LQFP-100)

### Common Peripheral Pin Assignments

| Function     | Pin   | AF    | Notes                    |
|--------------|-------|-------|--------------------------|
| USART2_TX    | PA2   | AF7   | Debug console            |
| USART2_RX    | PA3   | AF7   | Debug console            |
| SPI1_SCK     | PA5   | AF5   | SPI bus clock            |
| SPI1_MISO    | PA6   | AF5   | SPI bus data in          |
| SPI1_MOSI    | PA7   | AF5   | SPI bus data out         |
| I2C1_SCL     | PB6   | AF4   | I2C bus clock            |
| I2C1_SDA     | PB7   | AF4   | I2C bus data             |
| LED_GREEN    | PD12  | GPIO  | User LED (active high)   |
| LED_ORANGE   | PD13  | GPIO  | User LED (active high)   |
| LED_RED      | PD14  | GPIO  | User LED (active high)   |
| LED_BLUE     | PD15  | GPIO  | User LED (active high)   |
| USER_BUTTON  | PA0   | GPIO  | Blue user button         |
| USB_DM       | PA11  | AF10  | USB OTG FS               |
| USB_DP       | PA12  | AF10  | USB OTG FS               |
| CAN1_RX      | PD0   | AF9   | CAN bus receive          |
| CAN1_TX      | PD1   | AF9   | CAN bus transmit         |
| ADC1_IN0     | PA0   | Analog| ADC channel 0            |
| DAC1_OUT     | PA4   | Analog| DAC output               |
| TIM1_CH1     | PE9   | AF1   | PWM output               |
| SWD_IO       | PA13  | AF0   | Debug (SWDIO)            |
| SWD_CLK      | PA14  | AF0   | Debug (SWCLK)            |

### Memory Map

| Region       | Start        | End          | Size   |
|--------------|--------------|--------------|--------|
| Flash        | 0x0800_0000  | 0x080F_FFFF  | 1 MB   |
| SRAM1        | 0x2000_0000  | 0x2001_FFFF  | 128 KB |
| SRAM2        | 0x2002_0000  | 0x2002_FFFF  | 64 KB  |
| CCM          | 0x1000_0000  | 0x1000_FFFF  | 64 KB  |
| Peripherals  | 0x4000_0000  | 0x5FFF_FFFF  |        |

---

## nRF52840 (QFN-73)

### Common Peripheral Pin Assignments

| Function     | Pin   | Notes                         |
|--------------|-------|-------------------------------|
| UART_TX      | P0.06 | Default UART TX               |
| UART_RX      | P0.08 | Default UART RX               |
| SPI0_SCK     | P0.27 | SPI clock                     |
| SPI0_MOSI    | P0.26 | SPI data out                  |
| SPI0_MISO    | P1.08 | SPI data in                   |
| TWI0_SCL     | P0.11 | I2C clock                     |
| TWI0_SDA     | P0.12 | I2C data                      |
| LED1         | P0.13 | User LED 1                    |
| LED2         | P0.14 | User LED 2                    |
| LED3         | P0.15 | User LED 3                    |
| LED4         | P0.16 | User LED 4                    |
| BUTTON1      | P0.11 | User button 1                 |
| BUTTON2      | P0.12 | User button 2                 |
| USB_D+       | -     | Internal USB                  |
| USB_D-       | -     | Internal USB                  |
| NFC1         | P0.09 | NFC antenna 1                 |
| NFC2         | P0.10 | NFC antenna 2                 |
| RESET        | P0.18 | Active low reset              |

### Memory Map

| Region       | Start        | End          | Size    |
|--------------|--------------|--------------|---------|
| Flash        | 0x0000_0000  | 0x000F_FFFF  | 1 MB    |
| SRAM         | 0x2000_0000  | 0x2003_FFFF  | 256 KB  |
| Peripherals  | 0x4000_0000  | 0x4001_FFFF  |         |

---

## Raspberry Pi 4 (BCM2711)

### GPIO Header (40-pin)

| Pin | GPIO  | Function        | Alt Functions           |
|-----|-------|-----------------|-------------------------|
| 3   | GPIO2 | I2C1_SDA        | SDA1                    |
| 5   | GPIO3 | I2C1_SCL        | SCL1                    |
| 8   | GPIO14| UART0_TX        | TXD0                    |
| 10  | GPIO15| UART0_RX        | RXD0                    |
| 11  | GPIO17| General GPIO    | SPI1_CE1                |
| 12  | GPIO18| PCM_CLK / PWM0  |                         |
| 13  | GPIO27| General GPIO    |                         |
| 15  | GPIO22| General GPIO    |                         |
| 16  | GPIO23| General GPIO    |                         |
| 18  | GPIO24| General GPIO    |                         |
| 19  | GPIO10| SPI0_MOSI       |                         |
| 21  | GPIO9 | SPI0_MISO       |                         |
| 23  | GPIO11| SPI0_SCLK       |                         |
| 24  | GPIO8 | SPI0_CE0        |                         |
| 26  | GPIO7 | SPI0_CE1        |                         |
| 29  | GPIO5 | General GPIO    |                         |
| 31  | GPIO6 | General GPIO    |                         |
| 32  | GPIO12| PWM0            |                         |
| 33  | GPIO13| PWM1            |                         |
| 35  | GPIO19| PCM_FS / SPI1   |                         |
| 36  | GPIO16| General GPIO    |                         |
| 37  | GPIO26| General GPIO    |                         |
| 38  | GPIO20| SPI1_MOSI       |                         |
| 40  | GPIO21| SPI1_SCLK       |                         |

### Power Pins

| Pin(s)  | Function    |
|---------|-------------|
| 1, 17   | 3.3V Power  |
| 2, 4    | 5V Power    |
| 6,9,14,20,25,30,34,39 | Ground |
