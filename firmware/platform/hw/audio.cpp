// I2S + ES8311 bring-up lifted from claude-desktop-buddy-esp32/src/hw/audio.cpp
// (legacy i2s driver: allocates DMA in internal RAM). The tone synth is ours.
#include "platform/hw/audio.h"
#include "platform/board.h"
#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
#include <driver/i2s.h>
#include <math.h>
extern "C" {
#include "es8311.h"
}

static constexpr int SR = 32000;
static constexpr int CHUNK = 256;                   // samples per I2S write, 8 ms
static volatile int s_master = 32767;              // software master gain, Q15 (see audioVolume)

static es8311_handle_t s_codec = nullptr;
static volatile AudioMix s_target = { 0, 1000 };

static int16_t s_sine[1024];
static inline int16_t sineAt(uint32_t ph) { return s_sine[ph >> 22]; }       // 32-bit phase -> 1024 table
static inline uint32_t phaseInc(float hz) { return (uint32_t)(hz * 4294967296.0f / SR); }

static bool i2sInit() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SR;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = CHUNK;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = SR * 256;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCLK; pins.bck_io_num = PIN_I2S_BCLK; pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = PIN_I2S_DO; pins.data_in_num = I2S_PIN_NO_CHANGE;
  return i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK;
}

static bool codecInit() {
  s_codec = es8311_create((i2c_port_t)0, ES8311_ADDRRES_0);
  if (!s_codec) { Serial.println("audio: es8311_create failed"); return false; }
  const es8311_clock_config_t clk = { false, false, true, SR * 256, SR };
  if (es8311_init(s_codec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
  if (es8311_sample_frequency_config(s_codec, clk.mclk_frequency, clk.sample_frequency) != ESP_OK) return false;
  es8311_microphone_config(s_codec, false);
  es8311_voice_volume_set(s_codec, 85, nullptr);
  return true;
}

// Fixed point in the sample loop: the C6 has no FPU. Levels are Q15 (0..32767).
static inline int32_t q15(float v) { if (v < 0) v = 0; if (v > 1) v = 1; return (int32_t)(v * 32767.0f); }

// One sine voice. The level and the frequency glide towards the target once per chunk,
// so switching the tone on or off does not click.
static void synthTask(void*) {
  int16_t buf[CHUNK];
  uint32_t ph = 0;
  int32_t level = 0;                                 // Q15, smoothed
  float hz = 1000;
  while (true) {
    level += (q15(s_target.tone) - level) * 3 / 10;
    hz += (s_target.toneHz - hz) * 0.5f;
    uint32_t inc = phaseInc(hz);
    int32_t master = s_master;
    for (int i = 0; i < CHUNK; i++) {
      int32_t s = (sineAt(ph) * level) >> 15;       ph += inc;
      buf[i] = (int16_t)((s * master) >> 15);
    }
    size_t written;
    i2s_write(I2S_NUM_0, buf, sizeof(buf), &written, portMAX_DELAY);
  }
}

bool hwAudioInit() {
  for (int i = 0; i < 1024; i++) s_sine[i] = (int16_t)(32767.0f * sinf(i * 6.2831853f / 1024));
  if (!i2sInit())   { Serial.println("audio: I2S init failed"); return false; }
  if (!codecInit()) { Serial.println("audio: ES8311 init failed"); return false; }
  xTaskCreate(synthTask, "synth", 4096, nullptr, 5, nullptr);
  return true;
}

void audioSetMix(const AudioMix& m) { s_target.tone = m.tone; s_target.toneHz = m.toneHz; }

// The codec's own volume scale is not in dB, so the codec stays at a fixed level and a
// software master gain does the work. Loudness is heard in decibels: 0..100 is linear in
// dB over 36 dB (100 = full, 50 = -18 dB, 1 = -36 dB), and 0 is silent.
void audioVolume(int pct) {
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  s_master = pct == 0 ? 0 : (int)(32767.0f * powf(10.0f, (pct - 100) * 0.36f / 20.0f));
  if (s_codec) es8311_voice_volume_set(s_codec, pct == 0 ? 0 : 70, nullptr);
}
