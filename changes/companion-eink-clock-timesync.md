# Companion E-Ink Clock Time Sync

## Scope

This change applies to companion builds that use `examples/companion_radio/ui-new`
with an e-ink display, which is where the dedicated CLOCK page is currently enabled.

Target variants:

- `promicro_eink_spi`
- `promicro_eink_spi_v2`
- `heltec_e213`
- `heltec_e290`
- `thinknode_m1`
- `thinknode_m5`
- `wio-tracker-l1-eink`

## What changed

### 1. Advert-based time sync for companion builds

Added the repeater-style advert timestamp quorum sync to `examples/companion_radio/MyMesh.*`.

Behavior:

- Collect valid advert timestamps into a small ring buffer.
- Initial bootstrap mode:
  - use `3/5` quorum when the clock is still not reliable
- Drift correction mode:
  - use `7/10` quorum when the clock is already trusted or was previously synced
- Reject obviously bad timestamps outside:
  - `2020-01-01 UTC`
  - `2050-01-01 UTC`
- Apply drift correction only when the difference is meaningful.

Source priority implemented conservatively:

- hardware RTC starts as `RTC`
- contacts bootstrap starts as `CNT`
- app-set time starts as `APP`
- advert quorum sync starts as `ADV`
- if GPS is valid when a quorum sync is attempted, GPS remains authoritative and the source becomes `GPS`

To avoid the mesh immediately fighting a fresh app time-set, companion advert sync is held off
for 6 hours after `CMD_SET_DEVICE_TIME`.

### 2. CLOCK page source label

The e-ink CLOCK page now shows a compact time-source label at the bottom:

- `RTC`
- `CNT`
- `APP`
- `ADV`
- `GPS`

When the current source is advert sync, the last applied adjustment is also shown.

### 3. CLOCK page PM-only overlay fix

Fixed the CLOCK page "Do Not Disturb" leak for public/channel messages.

Previous behavior:

- PMs and channel messages were stored in the same unread queue.
- The CLOCK page PM overlay used the top unread entry without checking its type.
- If a PM arrived first and then a channel message arrived after it, the CLOCK page could show the public/channel message over the clock.

New behavior:

- unread preview entries now keep `is_pm`
- CLOCK page overlay peeks and consumes only the latest PM entry
- channel/public messages still queue normally for the message preview screen
- channel/public messages no longer break through the CLOCK page overlay path

## Files changed

- `examples/companion_radio/MyMesh.h`
- `examples/companion_radio/MyMesh.cpp`
- `examples/companion_radio/ui-new/UITask.cpp`

## Validation notes

I was able to verify the affected code paths statically in the repo, but I could not run a full
PlatformIO build in this environment because `pio/platformio` is not installed in PATH here.
