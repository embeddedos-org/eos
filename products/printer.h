/* 3D Printer / CNC product profile — Motor, Stepper, ADC (temp), Display, USB */
#ifndef EOS_PRODUCT_PRINTER_H
#define EOS_PRODUCT_PRINTER_H

#define EOS_PRODUCT_NAME    "printer"

/* Core peripherals */
#define EOS_ENABLE_GPIO      1
#define EOS_ENABLE_UART      1
#define EOS_ENABLE_SPI       1
#define EOS_ENABLE_I2C       1
#define EOS_ENABLE_TIMER     1

/* Extended peripherals */
#define EOS_ENABLE_ADC       1
#define EOS_ENABLE_DAC       1
#define EOS_ENABLE_PWM       1
#define EOS_ENABLE_USB       1
#define EOS_ENABLE_WIFI      1
#define EOS_ENABLE_DISPLAY   1
#define EOS_ENABLE_TOUCH     1
#define EOS_ENABLE_MOTOR     1
#define EOS_ENABLE_FLASH     1
#define EOS_ENABLE_DMA       1
#define EOS_ENABLE_WDT       1

/* Services */
#define EOS_ENABLE_OTA       1
#define EOS_ENABLE_FILESYSTEM 1

/* Frameworks */
#define EOS_ENABLE_NET       1
#define EOS_ENABLE_SENSOR    1
#define EOS_ENABLE_MOTOR_CTRL 1

#endif /* EOS_PRODUCT_PRINTER_H */
