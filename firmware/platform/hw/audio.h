// Audio: ES8311 codec over I2S and a synth task that streams one sine tone. The app sets
// the tone's level and frequency; the task runs at a higher priority than loop(), so a
// slow frame never interrupts the sound.
#pragma once
#include <stdint.h>

struct AudioMix {
  float tone;      // level 0..1
  float toneHz;    // frequency
};

bool hwAudioInit();
void audioSetMix(const AudioMix& m);
void audioVolume(int pct);   // 0..100, linear in dB; 0 = silent
