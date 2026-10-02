#include "touch.h"
#include <Arduino.h>
#include <Wire.h>
#include <TouchDrv.hpp>
#include "platform/board.h"
#include "i2c_lock.h"

static const uint8_t TOUCH_ADDR = 0x5A;
// Frames come every ~10 ms; the longest gap seen inside a steady hold was 22 ms.
static const uint32_t RELEASE_GAP_MS = 35;

static TouchDrvCST92xx s_touch;
static TouchEdgeFn s_edge;
static TouchTickFn s_tick;
static TouchTwoFingersFn s_two;

// Same protocol as the SensorLib driver (read 0xD000, then write the 0xAB ack), but the
// driver hides the gaps between frames and drops the frame types we need to see.
static bool readFrame(uint8_t* b, size_t n) {
  i2cLock();
  bool ok = false;
  Wire.beginTransmission(TOUCH_ADDR); Wire.write(0xD0); Wire.write(0x00);
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom(TOUCH_ADDR, (uint8_t)n) == n) {
    for (size_t i = 0; i < n; i++) b[i] = Wire.read();
    Wire.beginTransmission(TOUCH_ADDR); Wire.write(0xD0); Wire.write(0x00); Wire.write(0xAB);
    Wire.endTransmission();
    ok = true;
  }
  i2cUnlock();
  return ok;
}

// A real contact frame: byte 6 is the 0xAB marker, low nibble of byte 0 is the event
// (6 = pressed), byte 5 counts the points. Idle reads give 0xFF.. or the bare ack.
// The second finger's record starts at byte 7 (the first one's at byte 0), same layout.
static bool isPressedFrame(const uint8_t* b) {
  return b[6] == 0xAB && b[0] != 0xAB && (b[0] & 0x0F) == 0x06 && (b[5] & 0x7F) >= 1;
}

static bool hasSecondFinger(const uint8_t* b) { return (b[5] & 0x7F) >= 2 && (b[7] & 0x0F) == 0x06; }

static void touchTask(void*) {
  bool down = false, two = false;
  uint32_t tLast = 0;
  for (;;) {
    uint8_t b[15];
    uint32_t now = millis();
    if (readFrame(b, sizeof b) && isPressedFrame(b)) {
      tLast = now;
      if (!down) { down = true; two = false; if (s_edge) s_edge(true, now); }
      if (!two && hasSecondFinger(b)) { two = true; if (s_two) s_two(now); }
    } else if (down && (int32_t)(now - tLast) > (int32_t)RELEASE_GAP_MS) {
      down = false;
      if (s_edge) s_edge(false, tLast);
    }
    if (s_tick) s_tick(now);
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

bool hwTouchInit(TouchEdgeFn edge, TouchTickFn tick, TouchTwoFingersFn twoFingers) {
  s_edge = edge; s_tick = tick; s_two = twoFingers;
  s_touch.setPins(-1, PIN_TP_INT);
  // The SensorLib driver does the controller's start-up; reads after that are our own.
  if (!s_touch.begin(Wire, TOUCH_ADDR, PIN_I2C_SDA, PIN_I2C_SCL)) return false;
  xTaskCreate(touchTask, "touch", 4096, nullptr, 4, nullptr);   // above loop(), below the synth (5)
  return true;
}
