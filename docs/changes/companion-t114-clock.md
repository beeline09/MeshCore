# Companion T114 clock page

## Scope

Applies to companion-radio builds that use `examples/companion_radio/ui-new` with the
Heltec T114 ST7789 TFT display.

Affected target:

- `Heltec_t114_companion_radio_usb`
- `Heltec_t114_companion_radio_ble`

## Problem

The CLOCK page was originally tuned for 128x64 OLED displays. On Heltec V3 OLED,
`setTextSize(3)` uses the Adafruit GFX 6x8 bitmap font scaled to 18x24 logical pixels.
That makes the clock height `24 / 64 = 37.5%` of the display.

On the T114 TFT, the ST7789 driver rendered fixed Arial bitmap fonts directly in the
physical 240x135 framebuffer. A 24px font therefore occupied only `24 / 135 = 17.8%`
of the display height, so the clock looked much smaller than on OLED. The separate
date and time-source rows also did not fit cleanly in the available vertical space.

## Solution

The ST7789 driver now treats `setTextSize(3)` as a V3-compatible clock font mode:

- Uses the same `glcdfont6x8` bitmap source as the OLED/GFX path.
- Keeps the same logical 128x64 coordinate proportions.
- Scales logical pixels to the T114 physical display using:
  - `240 / 128` horizontally
  - `135 / 64` vertically

For `HH:MM`, this preserves the OLED proportions on T114:

- OLED V3: `18x24` glyph scale in `128x64`
- T114 TFT: about `169x51` physical pixels for the whole `HH:MM` string

The T114 CLOCK page layout now shows:

- large centered time
- larger centered date
- centered time-source label below the date

## Settings Persistence

The display settings added in the companion settings page are now persisted in
`NodePrefs` and written by `DataStore`:

- `ui_pm_clock_mode`
- `ui_clock_dim_mode`

These fields are appended to the existing companion prefs file, so older preference
files remain readable.

## Changed Files

| File | Change |
|---|---|
| `src/helpers/ui/ST7789Display.*` | V3-compatible scaled bitmap clock font mode for `setTextSize(3)` |
| `src/helpers/ui/ST7789Spi.h` | Exposes current font data for scaled text helpers |
| `src/helpers/ui/DisplayDriver.h` | Adds color-TFT display identification hook |
| `examples/companion_radio/ui-new/UITask.*` | T114-specific CLOCK layout and persisted UI setting wiring |
| `src/helpers/NodePrefs.h` | Adds persisted companion UI fields |
| `examples/companion_radio/DataStore.cpp` | Reads/writes the new UI preference bytes |
| `examples/companion_radio/MyMesh.cpp` | Initializes and constrains the new UI preference fields |

