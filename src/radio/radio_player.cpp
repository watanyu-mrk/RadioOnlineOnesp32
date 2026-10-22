#include "radio/radio_player.h"

#include "app_config.h"

void RadioPlayer::begin(StationManager *stations, AudioEngine *engine) {
  stations_ = stations;
  engine_ = engine;
}

bool RadioPlayer::playActive() {
  if (!stations_ || !engine_ || stations_->count() == 0) return false;
  deadSince_ = 0;
  bool ok = engine_->play(stations_->current().url);
  started_ = ok;
  return ok;
}

void RadioPlayer::stop() {
  if (engine_) engine_->stop();
  started_ = false;
  deadSince_ = 0;
}

void RadioPlayer::toggle() {
  if (!engine_) return;
  if (engine_->running()) {
    Serial.println("player    : STOP");
    stop();
  } else {
    Serial.println("player    : PLAY");
    playActive();
  }
}

int16_t RadioPlayer::changeStation(int8_t delta) {
  if (!stations_) return -1;
  int16_t idx = stations_->step(delta);
  if (idx < 0) return -1;
  Serial.printf("station   : %u/%u \"%s\"\n", (unsigned)idx + 1, (unsigned)stations_->count(),
                stations_->station((uint8_t)idx).name);
  playActive();
  return idx;
}

void RadioPlayer::tick() {
  if (!engine_) return;
  engine_->loop();

  if (!started_ || engine_->running()) {
    deadSince_ = 0;  // a live stream also reports running while it logs "slow stream"
    return;
  }
  if (deadSince_ == 0) {
    deadSince_ = millis();
    Serial.println("stream    : library reports stopped, waiting 15 s before retry");
  } else if (millis() - deadSince_ > STREAM_RETRY_MS) {
    Serial.println("stream    : still stopped, restarting");
    playActive();
  }
}
