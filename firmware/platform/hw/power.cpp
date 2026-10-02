// Lifted from claude-desktop-buddy-esp32/src/hw/power.cpp (2.16 paths only).
#include "platform/hw/power.h"
#include "platform/board.h"
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

static XPowersPMU s_pmu;

bool hwPowerInit() {
  if (!s_pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL)) {
    Serial.println("power: AXP2101 begin failed");
    return false;
  }
  s_pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
  s_pmu.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
  s_pmu.enableBattDetection();
  s_pmu.enableVbusVoltageMeasure();
  s_pmu.enableBattVoltageMeasure();
  s_pmu.enableTemperatureMeasure();

  // ALDO3 = display rail. ALDO2 drives DSI_PWR_EN (via R13); without it the
  // panel stays dark even with ALDO3 up. ALDO1/4 = mic bias / sensor rails.
  s_pmu.setDC1Voltage(3300);
  s_pmu.setALDO1Voltage(3300);
  s_pmu.setALDO2Voltage(3300);
  s_pmu.setALDO4Voltage(3300);
  s_pmu.enableALDO1();
  s_pmu.enableALDO2();
  s_pmu.enableALDO3();
  s_pmu.enableALDO4();

  // Power off on a 4 s PWRON hold: the only software-configurable
  // shutdown path on this board.
  s_pmu.writeRegister(0x22, 0b110);
  s_pmu.writeRegister(0x27, 0x10);

  s_pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  s_pmu.clearIrqStatus();
  return true;
}

void hwPowerPanelReset() {
  s_pmu.disableALDO3();
  delay(50);
  s_pmu.enableALDO3();
  delay(50);
}

uint16_t hwBatteryMilliVolts() { return s_pmu.getBattVoltage(); }
