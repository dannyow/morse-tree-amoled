// QMI8658 6-axis IMU via SensorLib. The accelerometer only, for the screen orientation.
#pragma once
bool hwImuInit();
// One raw sample in g. az > 0 when the screen faces UP (sign flipped vs the raw chip axis:
// raw Z is about -0.94 with the screen up on this board). Take i2cLock() around it.
bool hwImuAccelRaw(float* ax, float* ay, float* az);
