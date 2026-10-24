#include "audio/equalizer.h"

#include "audio/audio_engine.h"
#include "storage/settings_store.h"

namespace {

struct PresetDef {
  const char *name;
  int8_t gain[EQ_BAND_COUNT];  // low / mid / high, dB
};

// Values stay inside the library's -12..+12 dB range. They are deliberately moderate:
// a stream that is already hot plus +10 dB of bass would clip the class-D amp.
const PresetDef PRESETS[EQ_PRESET_COUNT] = {
    {"FLAT", {0, 0, 0}},
    {"POP", {3, 1, 2}},
    {"JAZZ", {2, -1, 2}},
    {"CLASSIC", {1, 0, 2}},
    {"ROCK", {5, -2, 4}},
    {"VOCAL", {-3, 5, 1}},
    {"BASS", {7, -1, 0}},
    {"TREBLE", {-1, 0, 6}},
};

const char *BAND_NAMES[EQ_BAND_COUNT] = {"LOW", "MID", "HIGH"};

int8_t clampGain(int v) {
  if (v > EQ_BAND_MAX) return EQ_BAND_MAX;
  if (v < -EQ_BAND_MAX) return -EQ_BAND_MAX;
  return (int8_t)v;
}

}  // namespace

const char *Equalizer::presetName(uint8_t idx) {
  return idx < EQ_PRESET_COUNT ? PRESETS[idx].name : "CUSTOM";
}

const char *Equalizer::bandName(uint8_t band) {
  return band < EQ_BAND_COUNT ? BAND_NAMES[band] : "?";
}

int8_t Equalizer::band(uint8_t band) const {
  return band < EQ_BAND_COUNT ? gain_[band] : 0;
}

void Equalizer::begin(AudioEngine *audio, SettingsStore *store) {
  audio_ = audio;
  store_ = store;

  uint8_t preset = 0;
  int8_t low = 0, mid = 0, high = 0;
  bool stored = store_ && store_->loadEq(preset, low, mid, high);
  if (stored && preset < EQ_PRESET_COUNT) {
    // A preset is authoritative: its table gains are the definition, so an old value
    // left by a different firmware cannot desync the label from the sound.
    preset_ = preset;
    for (uint8_t b = 0; b < EQ_BAND_COUNT; b++) gain_[b] = PRESETS[preset].gain[b];
  } else if (stored) {
    preset_ = EQ_PRESET_CUSTOM;
    gain_[0] = clampGain(low);
    gain_[1] = clampGain(mid);
    gain_[2] = clampGain(high);
  } else {
    preset_ = 0;
    for (uint8_t b = 0; b < EQ_BAND_COUNT; b++) gain_[b] = PRESETS[0].gain[b];
  }
  apply();
  Serial.printf("eq        : %s low=%d mid=%d high=%d%s\n", presetName(preset_),
                gain_[0], gain_[1], gain_[2], stored ? "" : " (default)");
}

void Equalizer::selectPreset(uint8_t idx) {
  if (idx >= EQ_PRESET_COUNT) return;
  preset_ = idx;
  for (uint8_t b = 0; b < EQ_BAND_COUNT; b++) gain_[b] = PRESETS[idx].gain[b];
  apply();
  save();
  Serial.printf("eq        : preset %s low=%d mid=%d high=%d\n", PRESETS[idx].name,
                gain_[0], gain_[1], gain_[2]);
}

void Equalizer::adjustBand(uint8_t band, int8_t delta) {
  if (band >= EQ_BAND_COUNT) return;
  int v = (int)gain_[band] + delta;
  int8_t next = clampGain(v);
  if (next == gain_[band]) return;
  gain_[band] = next;
  preset_ = EQ_PRESET_CUSTOM;  // no preset describes this curve any more
  apply();
  save();
  Serial.printf("eq        : custom %s=%+d dB (low=%d mid=%d high=%d)\n",
                BAND_NAMES[band], gain_[band], gain_[0], gain_[1], gain_[2]);
}

void Equalizer::apply() {
  if (audio_) audio_->setTone(gain_[0], gain_[1], gain_[2]);
}

void Equalizer::save() {
  if (store_) store_->saveEq(preset_, gain_[0], gain_[1], gain_[2]);
}
