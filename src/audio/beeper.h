#ifndef BEEPER_H
#define BEEPER_H

#include <Arduino.h>
#include <driver/i2s.h>

// Simple I2S beeper for ZX Spectrum
// Outputs audio buffer (312 samples per frame @ 15.6 KHz = 50 fps)
class Beeper {
private:
  i2s_port_t i2s_num = I2S_NUM_0;
  int sampleRate = 15600;  // 312 samples * 50 fps
  bool initialized = false;

public:
  bool init() {
    #ifdef I2S_SPEAKER_ENABLED
    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = sampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SPEAKER_SERIAL_CLOCK,
      .ws_io_num = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
      .data_out_num = I2S_SPEAKER_SERIAL_DATA,
      .data_in_num = I2S_PIN_NO_CHANGE
    };

    if (i2s_driver_install(i2s_num, &i2s_config, 0, NULL) != ESP_OK) {
      Serial.println("Failed to install I2S driver");
      return false;
    }

    if (i2s_set_pin(i2s_num, &pin_config) != ESP_OK) {
      Serial.println("Failed to set I2S pins");
      return false;
    }

    i2s_zero_dma_buffer(i2s_num);
    initialized = true;
    Serial.println("I2S Beeper initialized");
    return true;
    #else
    Serial.println("I2S Beeper disabled (no I2S_SPEAKER_ENABLED)");
    return false;
    #endif
  }

  // Write audio buffer (312 samples, 8-bit)
  // Convert to 16-bit stereo for I2S
  void write(const uint8_t *audioBuffer, size_t len) {
    if (!initialized) return;

    #ifdef I2S_SPEAKER_ENABLED
    int16_t samples[312];
    for (size_t i = 0; i < len && i < 312; i++) {
      // Convert 8-bit to 16-bit signed
      samples[i] = (int16_t)((audioBuffer[i] - 128) << 8);
    }

    size_t bytesWritten;
    i2s_write(i2s_num, samples, len * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    #endif
  }

  void silence() {
    #ifdef I2S_SPEAKER_ENABLED
    if (initialized) {
      i2s_zero_dma_buffer(i2s_num);
    }
    #endif
  }
};

#endif // BEEPER_H

