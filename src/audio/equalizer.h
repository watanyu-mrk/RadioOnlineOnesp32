#pragma once

// Tone control on top of AudioEngine's 3-band IIR filters.
//
// The library only exposes three bands (low shelf 500 Hz, peaking EQ 1.8 kHz, high
// shelf 6 kHz, each -12..+12 dB), so the presets are 3 numbers per band. Selecting a
// preset copies its gains; tapping a band +/- switches to CUSTOM (EQ_PRESET_CUSTOM) and
// keeps the last preset's other bands. Everything is persisted in NVS, so the tone
// survives a reboot and a station change (AudioEngine re-applies it on every play()).

#include <Arduino.h>

class AudioEngine;
class SettingsStore;

#define EQ_BAND_COUNT 3
#define EQ_BAND_MAX 12
#define EQ_PRESET_COUNT 8
#define EQ_PRESET_CUSTOM 0xFF

class Equalizer {
 public:
  void begin(AudioEngine *audio, SettingsStore *store);

  static const char *presetName(uint8_t idx);
  static const char *bandName(uint8_t band);

  uint8_t preset() const { return preset_; }
  int8_t band(uint8_t band) const;

  // Copies the preset's gains and pushes them to the audio engine.
  void selectPreset(uint8_t idx);

  // Steps one band by delta dB (-12..+12) and marks the result as CUSTOM.
  void adjustBand(uint8_t band, int8_t delta);

 private:
  void apply();
  void save();

  AudioEngine *audio_ = nullptr;
  SettingsStore *store_ = nullptr;
  uint8_t preset_ = 0;                       // EQ_PRESET_CUSTOM when hand-tuned
  int8_t gain_[EQ_BAND_COUNT] = {0, 0, 0};   // low, mid, high in dB
};
