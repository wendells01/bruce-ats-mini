#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "soc/soc_caps.h"
#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

// ===== ATS Mini Pin Mapping =====
// ESP32-S3-WROOM-1, 16MB Flash, 8MB PSRAM

// TFT Display (8-bit parallel SPI, ST7789/GC9307)
#define TFT_CS     6
#define TFT_DC     7
#define TFT_RST    5
#define TFT_WR     8
#define TFT_RD     9
#define TFT_BL     38   // Backlight PWM
// Data lines D0-D7: IO39-IO46
#define TFT_D0     39
#define TFT_D1     40
#define TFT_D2     41
#define TFT_D3     42
#define TFT_D4     43
#define TFT_D5     44
#define TFT_D6     45
#define TFT_D7     46

// Rotary Encoder
#define ENCODER_PIN_A    2
#define ENCODER_PIN_B    1
#define ENCODER_PUSH_BUTTON  21

// I2C (SI4732 on native firmware — not used in this port, but Wire needs defaults)
#define SDA 18
#define SCL 17

// System I2C bus (ATS Mini has no secondary I2C — aliases to main I2C pins)
#define SYS_I2C_SDA 18
#define SYS_I2C_SCL 17

// Grove connector I2C (ATS Mini has no Grove connector — map to free SPI header GPIOs)
#define GROVE_SDA 14
#define GROVE_SCL 13

// Serial bus (ATS Mini has no UART header — map to free SPI header GPIOs)
#define SERIAL_TX 11
#define SERIAL_RX 12

// GPS serial (ATS Mini has no GPS — same pins as serial bus)
#define GPS_SERIAL_TX 11
#define GPS_SERIAL_RX 12

// SPI (not used on ATS Mini — no SD/SPI peripherals, but SPI lib needs defaults)
#define SCK  12
#define MISO 13
#define MOSI 11
#define SS   10
// BadUSB CH9329 serial (ATS Mini has no CH9329 — these map to free SPI header GPIOs)
#define BAD_TX 11
#define BAD_RX 12

// Bruce SPI_*_PIN naming convention (used by utils.cpp device info, CC1101/NRF24 drivers)
#define SPI_SCK_PIN  SCK
#define SPI_MISO_PIN MISO
#define SPI_MOSI_PIN MOSI
#define SPI_SS_PIN   SS

// Audio (not used in this port)
#define PINAUDIO_MUTE    3
#define PINAUDIO_SPK_EN  10

// Battery ADC
#define BATTERY_PIN  4

// Free GPIOs (for external modules via SPI)
// IO11, IO12, IO13, IO14 available

// ===== Bruce integration macros =====
#define HAS_SCREEN 1
#define ROTATION 3
#define MINBRIGHT (uint8_t)1

#define HAS_BTN 1
#define BTN_ALIAS "\"Ok\""
#define BTN_PIN  ENCODER_PUSH_BUTTON
#define BTN_ACT  LOW

// Rotary encoder (key doubles as the single "OK" button)
#define HAS_ENCODER
#define ENCODER_INA ENCODER_PIN_A
#define ENCODER_INB ENCODER_PIN_B
#define ENCODER_KEY ENCODER_PUSH_BUTTON
#define SEL_BTN    ENCODER_KEY

// Battery is read directly from the ADC on GPIO4
#define ANALOG_BAT_PIN BATTERY_PIN

// TFT_eSPI configuration for the 8-bit parallel GC9307 (using ST7789 driver with custom init)
#define USER_SETUP_LOADED
#define ST7789_DRIVER 1
#define TFT_PARALLEL_8_BIT 1
#define TFT_WIDTH  170
#define TFT_HEIGHT 320
#define TFT_BACKLIGHT_ON 1
#define TFT_INVERSION_ON
#define TFT_RGB_ORDER TFT_RGB

// Not used on this board
#define TXLED -1
#define LED_ON LOW
#define LED_OFF HIGH

#define SDCARD_CS  -1
#define SDCARD_SCK -1
#define SDCARD_MISO -1
#define SDCARD_MOSI -1

#endif /* Pins_Arduino_h */
