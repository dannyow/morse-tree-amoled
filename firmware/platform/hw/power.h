#pragma once
#include <stdint.h>
// AXP2101 PMU. Must run before the display: the panel rails (ALDO2/ALDO3)
// come from it and the panel reset is an ALDO3 power-cycle.
bool hwPowerInit();
void hwPowerPanelReset();   // ALDO3 off 50 ms / on 50 ms
uint16_t hwBatteryMilliVolts();
