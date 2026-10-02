# The board: what turned out to matter

Notes on the Waveshare ESP32-C6-Touch-AMOLED-2.16, collected while writing this firmware.
Read them before changing anything in `firmware/platform/`.

## Bring-up

- Power first: `hwPowerInit()` before the display. The AXP2101 PMU powers the panel, and the
  panel reset is a power cycle of its rail (`hwPowerPanelReset()`).
- One I2C init (`Wire.begin` in `setup()`); every driver reuses the bus. The touch task and
  `loop()` both use it, so guard each access with `i2cLock()` / `i2cUnlock()`
  (`platform/hw/i2c_lock.h`).

## Display

- No full framebuffer: 480×480×2 bytes does not fit in RAM. A frame is rendered in 8-row
  stripes by a callback and pushed in ONE QSPI transaction (`hwDisplayPushFrame`). Many
  small draws blank the panel. Colours are RGB565 byte-swapped (`SWAP16`).
- The C6 has no FPU. Floats are fine once per frame, not once per pixel.
- Rotation is the panel's MADCTL register (`hwDisplayMadctl`). With the speaker edge up the
  upright picture is 0x90; 0xF0 puts the top of the picture on the right, 0x50 down, 0x30
  left. The corners of the panel are rounded and hide a bit of the picture, so keep text
  away from them.

## Keys and IMU

- KEY (GPIO10) and BOOT (GPIO9) are active low, PWR (GPIO18) is active high via an inverter.
- The QMI8658 reads +x toward the bottom of the screen and +y toward its left. A firm key
  press jolts it, so anything that reacts to motion should hold off for a moment after a
  key press.
- Its first samples after power-up are junk; wait about 0.5 s before trusting them.

## Touch

The CST9217 (I2C 0x5A, `platform/hw/touch.cpp`) streams a "pressed" frame about every 10 ms
while a finger is down, steady or moving, and sends no release frame: the stream just stops.
A release is therefore a gap. 35 ms works; the longest gap seen inside a hold was 22 ms.

The SensorLib driver acks each frame as it reads it and hides the gaps, so this firmware
reads raw frames in a task of its own. A display push blocks `loop()` for about 30 ms, which
would chop a hold into taps if touch were polled there.

These numbers were measured by logging every raw frame with its timestamp over serial.

## Audio

`audioSetMix()` sets the tone, `audioVolume()` is linear in dB (0 = silent). The speaker is
loud; keep the volume low on the bench.

## Timing and the toolchain

- Compare timestamps signed: `(int32_t)(now - stamp) > limit`, so the 49-day wrap of
  `millis()` does no harm.
- `delay()` counts RTOS ticks while `millis()` runs on esp_timer; end a precise wait with a
  spin on `millis()`.
- The Arduino builder appends `-Os` after the project's flags, so a hot loop needs its own
  `#pragma GCC optimize`.
