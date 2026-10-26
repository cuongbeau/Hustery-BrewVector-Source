#include "hardware/audio_service.h"

#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>

#include "board_pins.h"
#include "es8311.h"

namespace {
constexpr i2s_port_t AUDIO_PORT = I2S_NUM_0;
constexpr uint32_t SAMPLE_RATE = 16000;
es8311_handle_t codec = nullptr;
}

bool AudioService::begin(uint8_t volume) {
  pinMode(BoardPins::AUDIO_AMP_EN, OUTPUT);
  digitalWrite(BoardPins::AUDIO_AMP_EN, LOW);

  // Match the standalone ES3C28P audio test exactly. A stale I2S driver or a
  // second clock reconfiguration can leave MCLK present but the DAC silent.
  i2s_driver_uninstall(AUDIO_PORT);

  i2s_config_t config{};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = true;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 4096000;
  if (i2s_driver_install(AUDIO_PORT, &config, 0, nullptr) != ESP_OK)
    return false;

  i2s_pin_config_t pins{};
  pins.mck_io_num = BoardPins::AUDIO_MCLK;
  pins.bck_io_num = BoardPins::AUDIO_BCLK;
  pins.ws_io_num = BoardPins::AUDIO_LRCK;
  pins.data_out_num = BoardPins::AUDIO_DOUT;
  pins.data_in_num = BoardPins::AUDIO_DIN;
  if (i2s_set_pin(AUDIO_PORT, &pins) != ESP_OK) {
    i2s_driver_uninstall(AUDIO_PORT);
    return false;
  }
  i2s_zero_dma_buffer(AUDIO_PORT);
  i2s_start(AUDIO_PORT);

  Wire.beginTransmission(0x18);
  if (Wire.endTransmission() != 0) {
    Serial.println("[HMI audio] ES8311 not found at 0x18");
    i2s_driver_uninstall(AUDIO_PORT);
    return false;
  }
  Wire.setClock(100000);

  es8311_set_twowire(&Wire);
  codec = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  if (!codec) return false;
  es8311_clock_config_t clock{};
  clock.mclk_from_mclk_pin = true;
  clock.mclk_frequency = 4096000;
  clock.sample_frequency = SAMPLE_RATE;
  if (es8311_init(codec, &clock, ES8311_RESOLUTION_16,
                  ES8311_RESOLUTION_16) != ESP_OK)
    return false;

  if (es8311_voice_volume_set(codec, 100, nullptr) != ESP_OK ||
      es8311_voice_mute(codec, false) != ESP_OK) {
    Serial.println("[HMI audio] ES8311 unmute failed");
    return false;
  }

  tones_ = xQueueCreate(4, sizeof(Tone));
  if (!tones_ || xTaskCreate([](void* p){static_cast<AudioService*>(p)->run();},
      "hmi-audio", 4096, this, 1, nullptr) != pdPASS) {
    if(tones_){vQueueDelete(tones_);tones_=nullptr;}
    i2s_driver_uninstall(AUDIO_PORT);return false;
  }
  ready_ = true;
  setVolume(volume);
  digitalWrite(BoardPins::AUDIO_AMP_EN, LOW);
  Serial.println("[HMI audio] ES8311 ready, AMP_EN=LOW");
  return true;
}

void AudioService::setVolume(uint8_t volume) {
  volume_ = constrain(volume, 0, 100);
  // Codec stays at the known-good 100% setting; UI volume scales PCM.
}

void AudioService::playTone(uint16_t frequencyHz, uint16_t durationMs) {
  if (!ready_ || volume_ == 0) return;
  Tone tone{frequencyHz, durationMs};
  xQueueSend(tones_, &tone, 0); // Drop a click rather than delay touch processing.
}

void AudioService::run() {
  Tone tone{};
  for (;;) if (xQueueReceive(tones_, &tone, portMAX_DELAY) == pdTRUE) renderTone(tone);
}

void AudioService::renderTone(const Tone& tone) {
  // Only PCM uses the worker. The codec and touch share Wire on loopTask.
  const uint8_t volume = volume_.load();
  const uint16_t frequencyHz = tone.frequency, durationMs = tone.duration;
  constexpr size_t CHUNK_FRAMES = 256;
  int16_t samples[CHUNK_FRAMES * 2];
  uint32_t remaining = SAMPLE_RATE * durationMs / 1000;
  uint32_t phase = 0;
  const uint32_t phaseStep =
      static_cast<uint32_t>((static_cast<uint64_t>(frequencyHz) << 24) /
                            SAMPLE_RATE);
  while (remaining > 0) {
    const size_t frames = min<size_t>(remaining, CHUNK_FRAMES);
    for (size_t i = 0; i < frames; ++i) {
      const float angle = (phase & 0xFFFFFF) * (2.0f * PI / 16777216.0f);
      const float amplitude = 30000.0f * volume / 100.0f;
      const int16_t sample = static_cast<int16_t>(sinf(angle) * amplitude);
      samples[i * 2] = sample;
      samples[i * 2 + 1] = sample;
      phase += phaseStep;
    }
    size_t written = 0;
    if (i2s_write(AUDIO_PORT, samples, frames * 4, &written, pdMS_TO_TICKS(50)) != ESP_OK ||
        written != frames * 4) break;
    remaining -= frames;
  }
}

void AudioService::touchClick() { playTone(1800, 24); }

void AudioService::testTone() {
  playTone(880, 90);
  playTone(1320, 110);
}
