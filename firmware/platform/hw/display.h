#pragma once
#include <stdint.h>
#include "platform/board.h"

// Slice-streamed display path. There is NO full framebuffer (480×480×2 B =
// 450 KB > free SRAM). A frame is produced stripe by stripe by a render
// callback and streamed to the SH8601 inside ONE QSPI transaction, with CS held
// asserted for the whole frame. This panel goes black if the bus idles
// between separate draw calls (found in the claude-desktop-buddy-esp32 port), so never push a frame
// as many small draws.
constexpr int STRIPE_H = 8;                    // rows per stripe (7.5 KB buffer)

// Fill `dst` (LCD_W × rows, row-major) with physical rows [y0, y0+rows), in
// PANEL BYTE ORDER: RGB565 byte-swapped (MSB first on the wire). Use SWAP16().
// Called STRIPE_H rows at a time, top to bottom.
#define SWAP16(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))
// QSPI clock. ESP32 dividers give 80 or 40 MHz, nothing between. 80 MHz works on this
// panel; fall back to 40 if the picture breaks.
#define DISPLAY_QSPI_HZ 80000000
typedef void (*RenderFn)(void* ctx, uint16_t* dst, int y0, int rows);

bool hwDisplayInit();
void hwDisplayPushFrame(RenderFn fn, void* ctx);
void hwDisplayBrightness(uint8_t v);           // 0..255
// MADCTL 0x36. Rotation lives in the panel so the render code is always
// "logical up". Values from the pure-rotation coset of the vendor default
// 0x30: 0x30 / 0x50 / 0x90 / 0xF0. Settle the right one with a photo.
void hwDisplayMadctl(uint8_t v);
