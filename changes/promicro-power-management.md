# ProMicro nRF52 power management

## Summary

Enabled Phase 1 `NRF52_POWER_MANAGEMENT` for:

- `variants/promicro`
- `variants/promicro_eink_spi_v2`

This adds boot-time battery lockout, shutdown reason tracking, LPCOMP-based
voltage recovery wake, and VBUS wake through the shared `NRF52Board`
infrastructure already used by other supported nRF52 targets.

## Configuration

Both variants now use:

- `PWRMGT_VOLTAGE_BOOTLOCK = 3350`
- `PWRMGT_LPCOMP_AIN = 7`
- `PWRMGT_LPCOMP_REFSEL = 11`

## Rationale

- Both boards read battery on `D17`, which maps to `P0.31`, so `AIN7` is the
  correct LPCOMP input.
- The existing battery conversion uses `ADC_MULTIPLIER = 1.815f`, implying an
  effective divider close to `2.5x`.
- `3350 mV` is a conservative single-cell Li-ion startup threshold that helps
  avoid unstable boot, radio brownout, and flash corruption risk on a depleted
  cell.
- `REFSEL = 11` selects `7/16 VDD`, which should wake the board only after the
  cell recovers above the boot threshold, reducing the chance of wake/shutdown
  chatter compared with `3/8 VDD`.

## Board-specific behavior

- `promicro`: disables `SX126X_POWER_EN` before protective shutdown.
- `promicro_eink_spi_v2`: disables `PIN_GPS_EN` before protective shutdown.

Manual `powerOff()` behavior is left unchanged on both boards, so user-requested
power-off still enters plain `SYSTEMOFF` without arming LPCOMP wake.
