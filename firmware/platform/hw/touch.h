// Touch: CST9217 on the shared I2C bus, read as raw frames in its own task.
// The controller streams a "pressed" frame about every 10 ms while a finger is down and
// sends NO release frame: the stream just stops. A release is therefore a gap in frames,
// reported with the time of the last frame. Coordinates are not exposed; any contact
// anywhere counts. See the notes in CLAUDE.md before changing the timing.
#pragma once
#include <stdint.h>

// Called from the touch task. `down` true: first frame of a contact, t = its time.
// `down` false: contact ended, t = time of the last frame seen.
typedef void (*TouchEdgeFn)(bool down, uint32_t tMs);
// Called from the touch task every poll (about every 2 ms), for timers that need a clock.
typedef void (*TouchTickFn)(uint32_t nowMs);

// Called once per contact, from the touch task, the moment a second finger is seen. It comes
// after the `down` edge of the same contact; the contact ends later with the usual `up`.
typedef void (*TouchTwoFingersFn)(uint32_t tMs);

// Call after Wire.begin, the PMU init and the touch reset. Starts the task.
bool hwTouchInit(TouchEdgeFn edge, TouchTickFn tick, TouchTwoFingersFn twoFingers);
