// Lifted from claude-desktop-buddy-esp32/src/hw/imu.cpp.
#include "platform/hw/imu.h"
#include "platform/hw/i2c_lock.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "platform/board.h"
#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>

static SensorQMI8658 s_qmi;
static bool s_ok = false;

// The bus is already up (Wire.begin in setup()). SensorLib's begin() leaves the pins alone
// when none are passed; passing them again calls TwoWire::setPins() on an open bus, which
// logs an error line on every boot.
bool hwImuInit() {
  if (!s_qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS)) {
    Serial.println("imu: QMI8658 begin failed");
    return false;
  }
  Serial.printf("imu: WHO_AM_I=0x%02X chipID=0x%02X\n", s_qmi.whoAmI(), s_qmi.getChipID());
  // Reset first: without it the chip on this board returns saturated X/Y (stuck at +2g).
  s_qmi.reset();
  s_qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_500Hz, SensorQMI8658::LPF_OFF);
  s_qmi.enableAccelerometer();
  s_ok = true;
  return true;
}

bool hwImuAccelRaw(float* ax, float* ay, float* az) {
  if (!s_ok) return false;
  IMUdata d;
  if (!s_qmi.getAccelerometer(d.x, d.y, d.z)) return false;
  *ax = d.x; *ay = d.y; *az = -d.z;
  return true;
}

// ---- shared I2C lock ----
static SemaphoreHandle_t s_i2c = nullptr;
static void i2cLockInit() { if (!s_i2c) s_i2c = xSemaphoreCreateRecursiveMutex(); }
void i2cLock()   { i2cLockInit(); xSemaphoreTakeRecursive(s_i2c, portMAX_DELAY); }
void i2cUnlock() { xSemaphoreGiveRecursive(s_i2c); }
