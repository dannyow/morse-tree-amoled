// One recursive mutex around the shared I2C bus (AXP2101, ES8311, CST9217, QMI8658, ...).
// The touch task reads the controller every few ms while loop() reads the accelerometer;
// Wire is not thread-safe, so every I2C user takes this lock.
#pragma once
void i2cLock();
void i2cUnlock();
