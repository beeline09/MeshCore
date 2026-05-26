/*
 * variant.h - ProMicro NRF52840 + WeAct Epaper 2.13" + E22/E22P/RA-62
 * Shared SPI bus (D12/D14/D15) for both radio and e-ink display.
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

#define PIN_EXT_VCC          (21)
#define EXT_VCC              (PIN_EXT_VCC)

#define BATTERY_PIN          (17)
#define ADC_RESOLUTION       12

////////////////////////////////////////////////////////////////////////////////
// Number of pins

#define PINS_COUNT           (23)
#define NUM_DIGITAL_PINS     (23)
#define NUM_ANALOG_INPUTS    (3)
#define NUM_ANALOG_OUTPUTS   (0)

////////////////////////////////////////////////////////////////////////////////
// UART pin definition (required by framework Uart.cpp even if Serial1 is unused)

#define PIN_SERIAL1_TX       (1)   // D1 = P0.06 (free)
#define PIN_SERIAL1_RX       (0)   // D0 = P0.08 (free)

////////////////////////////////////////////////////////////////////////////////
// I2C pin definition (defined for compatibility but Wire not used in this variant)

#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (6)
#define PIN_WIRE_SCL         (7)

////////////////////////////////////////////////////////////////////////////////
// SPI pin definition
// Single shared SPI bus for radio (E22/E22P/RA-62) and e-ink display.
// Pins are set at runtime via SPI.setPins() in CustomSX1262/CustomLLCC68::std_init().

#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_SCK          (12)   // D12 = P1.11
#define PIN_SPI_MISO         (15)   // D15 = P0.02
#define PIN_SPI_MOSI         (14)   // D14 = P1.15

#define PIN_SPI_NSS          (13)   // D13 = P1.13 (radio CS)

////////////////////////////////////////////////////////////////////////////////
// Builtin LEDs

#define PIN_LED              (22)
#define LED_PIN              PIN_LED
#define LED_BLUE             PIN_LED
#define LED_BUILTIN          PIN_LED
#define LED_STATE_ON         1

////////////////////////////////////////////////////////////////////////////////
// Builtin buttons

#define PIN_BUTTON1          (6)    // D6 = P1.00 — KEY SELECT
#define BUTTON_PIN           PIN_BUTTON1

// GxEPD2 requires SCK/MISO/MOSI globals (used by GxEPD2_1248c which is compiled
// even though it's never used on this target).
extern const int SCK;
extern const int MISO;
extern const int MOSI;
