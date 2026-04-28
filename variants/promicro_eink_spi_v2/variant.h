/*
 * variant.h - ProMicro NRF52840 + WeAct Epaper 2.13" + E22P (SX1262)
 * LoRa uses custom SPI pins via SPI.setPins() — default SPI0 pins are free at runtime.
 * D20 remapped to P1.07 (P1.05 not routed on this board).
 */

#pragma once

#include "WVariant.h"

////////////////////////////////////////////////////////////////////////////////
// Low frequency clock source

#define VARIANT_MCK       (64000000ul)

//#define USE_LFXO      // 32.768 kHz crystal oscillator
#define USE_LFRC    // 32.768 kHz RC oscillator

////////////////////////////////////////////////////////////////////////////////
// Power

#define BATTERY_PIN          (17)
#define ADC_RESOLUTION       12

// nRF52 power management settings for a single-cell Li-ion/LiPo battery.
// D17 maps to P0.31, which is AIN7 for LPCOMP wake measurements.
// 3.35V boot lockout prevents unstable boot on a depleted cell. 7/16 VDD
// yields a recovery wake threshold slightly above the boot threshold with the
// board's ~2.5x battery divider.
#define PWRMGT_VOLTAGE_BOOTLOCK 3350
#define PWRMGT_LPCOMP_AIN 7
#define PWRMGT_LPCOMP_REFSEL 11

////////////////////////////////////////////////////////////////////////////////
// Number of pins

#define PINS_COUNT           (23)
#define NUM_DIGITAL_PINS     (23)
#define NUM_ANALOG_INPUTS    (3)
#define NUM_ANALOG_OUTPUTS   (0)

////////////////////////////////////////////////////////////////////////////////
// UART pin definition (GPS via Serial1)

#define PIN_SERIAL1_TX       (1)
#define PIN_SERIAL1_RX       (0)

////////////////////////////////////////////////////////////////////////////////
// I2C pin definition (not used — defined for compatibility)

#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (6)
#define PIN_WIRE_SCL         (7)

////////////////////////////////////////////////////////////////////////////////
// SPI pin definition
// Note: LoRa uses custom pins D12/D14/D15 via SPI.setPins() in CustomSX1262.
// Default SPI0 pins (D2-D5) are free at runtime.
// SPI1 (D18/D19/D20) is used for the e-ink display.

#define SPI_INTERFACES_COUNT 2

#define PIN_SPI_SCK          (2)
#define PIN_SPI_MISO         (3)
#define PIN_SPI_MOSI         (4)
#define PIN_SPI_NSS          (5)

#define PIN_SPI1_SCK         (18)
#define PIN_SPI1_MISO        (19)
#define PIN_SPI1_MOSI        (20)

////////////////////////////////////////////////////////////////////////////////
// Builtin LEDs

#define PIN_LED              (22)
#define LED_PIN              PIN_LED
#define LED_BLUE             PIN_LED
#define LED_BUILTIN          PIN_LED
#define LED_STATE_ON         1

////////////////////////////////////////////////////////////////////////////////
// Builtin buttons

#define PIN_BUTTON1          (6)
#define BUTTON_PIN           PIN_BUTTON1

// GxEPD2 requires SCK/MISO/MOSI globals (used by GxEPD2_1248c which is compiled
// even though it's never used on this target).
extern const int SCK;
extern const int MISO;
extern const int MOSI;
