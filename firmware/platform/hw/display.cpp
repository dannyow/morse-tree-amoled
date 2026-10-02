// SH8601 over QSPI via Arduino_GFX. Init sequence lifted from
// claude-desktop-buddy-esp32/src/hw/display.cpp (which lifted it from the
// Waveshare LVGL demo bsp_lvgl_port.cpp). Push path is our own stripe streamer.
#include "platform/hw/display.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

static Arduino_DataBus* s_bus = nullptr;
static Arduino_SH8601*  s_gfx = nullptr;
static uint16_t         s_stripe[LCD_W * STRIPE_H];

// Re-runs after Arduino_SH8601's basic init to override pixel format, MADCTL,
// vendor power/gamma regs, full 480×480 window, max brightness, DISPON.
// Without this the panel inits but stays dark.
static void sh8601_vendor_init(Arduino_DataBus* bus) {
  static const uint8_t init_ops[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,            // SLPOUT
    END_WRITE,
    DELAY, 120,
    BEGIN_WRITE,
    WRITE_C8_D8, 0xFE, 0x20,          // page select MFR
    WRITE_C8_D8, 0x19, 0x10,
    WRITE_C8_D8, 0x1C, 0xA0,
    WRITE_C8_D8, 0xFE, 0x00,          // back to USER page
    WRITE_C8_D8, 0xC4, 0x80,
    WRITE_C8_D8, 0x3A, 0x55,          // RGB565
    WRITE_C8_D8, 0x35, 0x00,          // tearing line
    WRITE_C8_D8, 0x36, 0x30,          // MADCTL vendor default
    WRITE_C8_D8, 0x53, 0x20,          // brightness control on
    WRITE_C8_D8, 0x51, 0xFF,          // brightness max
    WRITE_C8_D8, 0x63, 0xFF,
    WRITE_COMMAND_8, 0x2A, WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0xDF,  // col 0..479
    WRITE_COMMAND_8, 0x2B, WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0xDF,  // row 0..479
    WRITE_COMMAND_8, 0x29,            // DISPON
    END_WRITE,
    DELAY, 50,
  };
  bus->batchOperation(init_ops, sizeof(init_ops));
}

bool hwDisplayInit() {
  s_bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_SDIO0,
                                PIN_LCD_SDIO1, PIN_LCD_SDIO2, PIN_LCD_SDIO3);
  s_gfx = new Arduino_SH8601(s_bus, GFX_NOT_DEFINED, 0, LCD_W, LCD_H);
  if (!s_gfx->begin(DISPLAY_QSPI_HZ)) { Serial.println("display: SH8601 begin failed"); return false; }
  sh8601_vendor_init(s_bus);
  s_gfx->fillScreen(0x0000);
  return true;
}

void hwDisplayBrightness(uint8_t v) { s_gfx->setBrightness(v); }

void hwDisplayMadctl(uint8_t v) {
  s_bus->beginWrite();
  s_bus->writeC8D8(0x36, v);
  s_bus->endWrite();
}

void hwDisplayPushFrame(RenderFn fn, void* ctx) {
  s_gfx->startWrite();
  s_gfx->writeAddrWindow(0, 0, LCD_W, LCD_H);
  for (int y = 0; y < LCD_H; y += STRIPE_H) {
    fn(ctx, s_stripe, y, STRIPE_H);   // already in panel byte order
    s_gfx->writeBytes((uint8_t*)s_stripe, sizeof(s_stripe));
  }
  s_gfx->endWrite();
}
