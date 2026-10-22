#include "audio/audio_engine.h"

#include "Audio.h"
#include "board_pins.h"

// One library instance; the engine is the only code that sees it.
static Audio audio;

bool AudioEngine::begin() {
  // Library events: real stream metadata only, never a synthesised name.
  audio.audio_info_callback = [this](Audio::msg_t m) {
    const char *type = (m.e < Audio::eventStr.size()) ? Audio::eventStr[m.e] : "?";
    Serial.printf("event     : %s: %s%s%s\n", type, m.msg ? m.msg : "", m.s ? " | " : "",
                  m.s ? m.s : "");
    if (m.e == Audio::evt_name && m.msg) {
      snprintf(station_, sizeof(station_), "%s", m.msg);
    } else if (m.e == Audio::evt_streamtitle && m.msg) {
      snprintf(title_, sizeof(title_), "%s", m.msg);
    }
  };
  return true;
}

bool AudioEngine::play(const char *url) {
  if (!url || !url[0]) return false;
  Serial.printf("stream    : url=%s\n", url);
  if (!pinsReady_) {
    audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
    pinsReady_ = true;
  }
  audio.setVolume(muted_ ? 0 : volume_);
  station_[0] = 0;  // the next station announces itself; do not show the old name
  title_[0] = 0;
  bool ok = audio.connecttohost(url);
  Serial.printf("stream    : connecttohost -> %s\n", ok ? "accepted" : "REJECTED");
  started_ = ok;
  return ok;
}

void AudioEngine::stop() {
  audio.stopSong();
  started_ = false;
}

void AudioEngine::loop() { audio.loop(); }

bool AudioEngine::running() { return audio.isRunning(); }

void AudioEngine::setVolume(uint8_t level) {
  volume_ = level > AUDIO_VOLUME_MAX ? AUDIO_VOLUME_MAX : level;
  audio.setVolume(muted_ ? 0 : volume_);
}

void AudioEngine::setMuted(bool muted) {
  muted_ = muted;
  audio.setVolume(muted_ ? 0 : volume_);
}

uint32_t AudioEngine::bitrate() { return audio.getBitRate(); }

const char *AudioEngine::codec() { return audio.getCodecname(); }

uint32_t AudioEngine::sampleRate() { return audio.getSampleRate(); }

uint8_t AudioEngine::channels() { return audio.getChannels(); }

uint32_t AudioEngine::playedSeconds() { return audio.getAudioCurrentTime(); }

uint32_t AudioEngine::bufferBytes() { return audio.inBufferFilled(); }

uint16_t AudioEngine::vu() { return audio.getVUlevel(); }
