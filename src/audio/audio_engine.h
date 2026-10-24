#pragma once

// Wrapper around ESP32-audioI2S: the only module that touches the library object.
//
// It owns the once-per-boot setPinout() guard, the volume/mute state and the ICY
// metadata, and exposes plain getters so the UI never sees Audio.h.
//
// setPinout() allocates the I2S channel, so it must run exactly once per boot. Calling it
// again for every station change made i2s_new_channel() fail with "no available channel
// found"; the library then stored m_f_I2S_init = false and its audio task skipped all
// decoding - the stream kept filling the input buffer (played=0s, silence) until a reboot.

#include <Arduino.h>

#include "app_config.h"

class AudioEngine {
 public:
  // Creates the I2S channel and installs the metadata callback. Call once in setup().
  bool begin();

  bool play(const char *url);
  void stop();  // also clears the metadata of the station that is going away
  void loop();

  bool running();
  bool starting() const { return started_; }

  void setVolume(uint8_t level);
  void setMuted(bool muted);
  uint8_t volume() const { return volume_; }
  bool muted() const { return muted_; }

  // 3-band tone control of the library's IIR post-processing (low shelf ~500 Hz,
  // peaking EQ ~1.8 kHz, high shelf ~6 kHz). Values are dB, clamped to -12..+12.
  // The library recomputes its coefficients on every sample-rate change, so applying
  // the stored gains once per play() is enough - the tone survives station changes.
  void setTone(int8_t low, int8_t mid, int8_t high);

  const char *stationName() const { return station_; }
  const char *title() const { return title_; }

  uint32_t bitrate();
  const char *codec();
  uint32_t sampleRate();
  uint8_t channels();
  uint32_t playedSeconds();
  uint32_t bufferBytes();
  uint16_t vu();

 private:
  bool pinsReady_ = false;
  bool started_ = false;
  uint8_t volume_ = AUDIO_VOLUME_DEFAULT;
  bool muted_ = false;
  int8_t toneLow_ = 0;
  int8_t toneMid_ = 0;
  int8_t toneHigh_ = 0;
  char station_[40] = {0};
  char title_[64] = {0};
};
