# Third-party code and content

| path | what | licence | source |
|---|---|---|---|
| `firmware/platform/board.h`, `hw/audio.cpp`, `hw/display.cpp`, `hw/imu.cpp`, `hw/power.cpp` (parts marked "lifted from") | board pin map and hardware bring-up, from the ESP32 port of Anthropic's claude-desktop-buddy | MIT, Copyright 2026 Anthropic, PBC; notice in `firmware/platform/hw/NOTICE-claude-desktop-buddy-esp32.md` | https://github.com/vthinkxie/claude-desktop-buddy-esp32 |
| `firmware/lib/ES8311/` | ES8311 codec driver, Espressif Systems | Apache-2.0 | via the claude-desktop-buddy-esp32 board bring-up |
| (`lib_deps`) | GFX Library for Arduino, XPowersLib, SensorLib | BSD / MIT / MIT | PlatformIO registry |
