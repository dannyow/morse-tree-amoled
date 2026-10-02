// board.h: Waveshare ESP32-C6-Touch-AMOLED-2.16 pin map.
// Lifted from claude-desktop-buddy-esp32/src/boards/board_waveshare_esp32c6_touch_amoled_2_16.h
// (XiaoZhi v2.2.5 board def + schematic verification). Trust this over the wiki.
#pragma once

#define LCD_W 480
#define LCD_H 480

// QSPI to SH8601
#define PIN_LCD_SDIO0  1
#define PIN_LCD_SDIO1  2
#define PIN_LCD_SDIO2  3
#define PIN_LCD_SDIO3  4
#define PIN_LCD_SCLK   0
#define PIN_LCD_CS     15
// No LCD reset GPIO: the panel is reset by power-cycling AXP2101 ALDO3.

// I2C bus (shared: AXP2101, ES8311, ES7210, CST9217, QMI8658, PCF85063)
#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   7

// Touch CST9217 @ 0x5A
#define PIN_TP_INT    5
#define PIN_TP_RESET  11

// I2S to ES8311
#define PIN_I2S_MCLK  19
#define PIN_I2S_BCLK  20
#define PIN_I2S_WS    22
#define PIN_I2S_DI    21
#define PIN_I2S_DO    23

// IMU QMI8658 interrupts (polled for now)
#define PIN_QMI_INT1  16
#define PIN_QMI_INT2  17

// Keys. PWR is ACTIVE-HIGH (BSS138 inverter, also AXP PWRON). Others active-low.
#define PIN_KEY_PWR   18
#define PIN_KEY_IO10  10
#define PIN_KEY_BOOT  9
