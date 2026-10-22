#pragma once

// Radio manager: owns the "which station is playing" policy on top of AudioEngine and
// StationManager, including the restart watchdog. The sketch asks for actions
// (play/stop/next/prev) and calls tick() from loop().

#include <Arduino.h>

#include "audio/audio_engine.h"
#include "radio/station_manager.h"

class RadioPlayer {
 public:
  void begin(StationManager *stations, AudioEngine *engine);

  bool playActive();             // stream the active station
  void stop();                   // stop decoding, cancel the watchdog
  void toggle();                 // play/stop, used by the PLAY button and 's'
  bool started() const { return started_; }

  // Wraps around the station list and starts playing. Returns the new index or -1.
  int16_t changeStation(int8_t delta);

  // Feeds the library and restarts a stream it reports dead, but only after
  // STREAM_RETRY_MS so a buffer that is still filling is not thrown away.
  void tick();

 private:
  StationManager *stations_ = nullptr;
  AudioEngine *engine_ = nullptr;
  bool started_ = false;
  uint32_t deadSince_ = 0;
};
