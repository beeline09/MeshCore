/*
 * variant.h — Darktec (железо класса ProMicro nRF52840)
 */

#pragma once

#include "WVariant.h"

////////////////////////////////////////////////////////////////////////////////
// Источник низкочастотных часов

#define VARIANT_MCK       (64000000ul)

//#define USE_LFXO      // кварц 32.768 кГц
#define USE_LFRC    // RC-генератор 32.768 кГц

////////////////////////////////////////////////////////////////////////////////
// Питание

#define PIN_EXT_VCC          (21)
#define EXT_VCC              (PIN_EXT_VCC)

#define BATTERY_PIN          (17)
#define ADC_RESOLUTION       12

// Химия / число ячеек → % на UI и пороги защиты питания.
// См. battery_chemistry.h (BATTERY_CHEMISTRY, BATTERY_CELLS). Пакет ≤ 5 В.
#include "battery_chemistry.h"

// Радио по умолчанию: 869.075 / 62.5 / SF8 / CR8 / 22 дБм.
#include "radio_defaults.h"

// D17 → P0.31 → AIN7 для измерений LPCOMP.
#define PWRMGT_LPCOMP_AIN        7

////////////////////////////////////////////////////////////////////////////////
// Число пинов

#define PINS_COUNT           (23)
#define NUM_DIGITAL_PINS     (23)
#define NUM_ANALOG_INPUTS    (3)
#define NUM_ANALOG_OUTPUTS   (0)

////////////////////////////////////////////////////////////////////////////////
// UART

#define PIN_SERIAL1_TX       (1)
#define PIN_SERIAL1_RX       (0)

////////////////////////////////////////////////////////////////////////////////
// I2C

#define WIRE_INTERFACES_COUNT 2

#define PIN_WIRE_SDA         (6)
#define PIN_WIRE_SCL         (7)
#define PIN_WIRE1_SDA        (13)
#define PIN_WIRE1_SCL        (14)

////////////////////////////////////////////////////////////////////////////////
// SPI

#define SPI_INTERFACES_COUNT 2

#define PIN_SPI_SCK          (2)
#define PIN_SPI_MISO         (3)
#define PIN_SPI_MOSI         (4)

#define PIN_SPI_NSS          (5)

#define PIN_SPI1_SCK         (18)
#define PIN_SPI1_MISO        (19)
#define PIN_SPI1_MOSI        (20)

////////////////////////////////////////////////////////////////////////////////
// Встроенные светодиоды

#define PIN_LED              (22)
#define LED_PIN              PIN_LED
#define LED_BLUE             PIN_LED
#define LED_BUILTIN          PIN_LED
#define LED_STATE_ON         1

////////////////////////////////////////////////////////////////////////////////
// Встроенные кнопки

#define PIN_BUTTON1          (6)
#define BUTTON_PIN           PIN_BUTTON1
