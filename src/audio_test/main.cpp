// Phase 3 - audio path test (no Wi-Fi, no stream, no decoding)
//
// Purpose: prove that GPIO1/2/40 actually reach the NS4168 amplifier and that the
// speaker connector works, before any radio code depends on it.
//
// The vendor manual says audio needs 0R resistors R25/R26/R27 moved to R21/R22/R23
// (the same three lines drive the relays by default). This sketch is the test that
// decides whether that move is required on this specific board: if the speaker stays
// silent through every phase below, the I2S lines are still routed to the relays.
//
// I2S bus is stereo 16-bit/44.1 kHz. NS4168 is a mono BTL amplifier, so both slots
// carry the same sample; an external I2S DAC (e.g. PCM5102A) can later tap the same
// three lines and get a real stereo line out without changing the bus setup.

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <math.h>
#include "board_pins.h"

static i2s_chan_handle_t txChan = nullptr;

// Keep this low: NS4168 has fixed gain and the stock speaker is 1.5 W.
static const float AMPLITUDE = 0.08f;  // 8% of full scale (~ -22 dBFS)
static const int SAMPLE_RATE = 44100;
static const int FRAMES_PER_CHUNK = 256;

static int16_t chunk[FRAMES_PER_CHUNK * 2];

static void fillTone(float freq, uint32_t &phaseAcc) {
  for (int i = 0; i < FRAMES_PER_CHUNK; i++) {
    float s = (freq > 0.0f) ? sinf(2.0f * PI * freq * (float)phaseAcc / (float)SAMPLE_RATE) : 0.0f;
    int16_t v = (int16_t)(s * AMPLITUDE * 32767.0f);
    chunk[i * 2 + 0] = v;
    chunk[i * 2 + 1] = v;
    phaseAcc++;
  }
}

static void writeChunk() {
  size_t written = 0;
  i2s_channel_write(txChan, chunk, sizeof(chunk), &written, portMAX_DELAY);
}

// Plays `ms` milliseconds of a tone (freq <= 0 means silence).
static void playTone(const char *label, float freq, uint32_t ms) {
  Serial.printf("PHASE %-22s freq=%7.1f Hz  dur=%lums\n", label, freq, (unsigned long)ms);
  uint32_t phaseAcc = 0;
  uint32_t chunks = (uint32_t)((uint64_t)SAMPLE_RATE * ms / 1000 / FRAMES_PER_CHUNK);
  for (uint32_t c = 0; c < chunks; c++) {
    fillTone(freq, phaseAcc);
    writeChunk();
  }
}

static void playSweep(uint32_t ms) {
  Serial.printf("PHASE %-22s 200 -> 2000 Hz     dur=%lums\n", "sweep", (unsigned long)ms);
  uint32_t phaseAcc = 0;
  uint32_t chunks = (uint32_t)((uint64_t)SAMPLE_RATE * ms / 1000 / FRAMES_PER_CHUNK);
  for (uint32_t c = 0; c < chunks; c++) {
    float t = (float)c / (float)chunks;
    float freq = 200.0f + 1800.0f * t;
    fillTone(freq, phaseAcc);
    writeChunk();
  }
}

static bool i2sInit() {
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chanCfg.auto_clear_after_cb = true;  // output silence on underrun, never stale data
  esp_err_t err = i2s_new_channel(&chanCfg, &txChan, nullptr);
  if (err != ESP_OK) {
    Serial.printf("i2s_new_channel failed: %s\n", esp_err_to_name(err));
    return false;
  }

  i2s_std_config_t stdCfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = (gpio_num_t)I2S_BCLK,
          .ws = (gpio_num_t)I2S_LRCLK,
          .dout = (gpio_num_t)I2S_DOUT,
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };

  err = i2s_channel_init_std_mode(txChan, &stdCfg);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_init_std_mode failed: %s\n", esp_err_to_name(err));
    return false;
  }
  err = i2s_channel_enable(txChan);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_enable failed: %s\n", esp_err_to_name(err));
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("==================================================");
  Serial.println(" Phase 3 - I2S / NS4168 audio path test");
  Serial.println("==================================================");
  Serial.printf("I2S       : BCLK=GPIO%d  LRCLK=GPIO%d  DOUT=GPIO%d  (no MCLK)\n",
                I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  Serial.printf("format    : 44100 Hz, 16-bit, stereo (identical L/R), amp = %.0f%% FS\n",
                AMPLITUDE * 100.0f);
  Serial.println("note      : these three lines also drive relays L3/L2/L1 by default;");
  Serial.println("            clicking at the start of each phase is expected.");

  if (!i2sInit()) {
    Serial.println("RESULT    : I2S INIT FAILED - no audio possible");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("I2S       : ready");
  Serial.println("RESULT    : listen to the speaker - 4 phases loop forever");
}

void loop() {
  playTone("silence", 0.0f, 1200);
  playTone("tone 440 Hz (A4)", 440.0f, 3000);
  playTone("silence", 0.0f, 800);
  playTone("tone 1000 Hz", 1000.0f, 3000);
  playTone("silence", 0.0f, 800);
  playSweep(4000);
  playTone("silence", 0.0f, 1500);

  Serial.printf("heap: free=%u  psram free=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
}
