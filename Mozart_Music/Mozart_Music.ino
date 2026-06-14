#include <Arduino.h>
#include <driver/i2s_std.h>

// I2S Pin Configuration
#define I2S_BCLK     7
#define I2S_LRC      8
#define I2S_DOUT     9

#define SAMPLE_RATE  22050  // 22.05 kHz audio sample rate
#define VOLUME       15000   // Max is 32767

i2s_chan_handle_t tx_handle;

// Musical note frequencies (in Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_REST  0

// Mozart - "Ah vous dirai-je, Maman"
int melody[] = {
  NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4,
  NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4,
  NOTE_G4, NOTE_G4, NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4,
  NOTE_G4, NOTE_G4, NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4
};

int durations[] = {
  4, 4, 4, 4, 4, 4, 2,
  4, 4, 4, 4, 4, 4, 2,
  4, 4, 4, 4, 4, 4, 2,
  4, 4, 4, 4, 4, 4, 2
};

void initI2S() {
  // FIX 1: Updated macro name for ESP32 Core v3.x
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, &tx_handle, NULL);

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_BCLK,
      .ws   = (gpio_num_t)I2S_LRC,
      .dout = (gpio_num_t)I2S_DOUT,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
    }
  };

  // FIX 2: Updated function name for ESP32 Core v3.x
  i2s_channel_init_std_mode(tx_handle, &std_cfg);
  i2s_channel_enable(tx_handle);
}

void playTone(int frequency, int duration_ms) {
  if (frequency == NOTE_REST) {
    delay(duration_ms);
    return;
  }

  unsigned long num_samples = (SAMPLE_RATE * duration_ms) / 1000;
  int16_t *buffer = (int16_t *)malloc(num_samples * sizeof(int16_t));
  
  if (buffer == NULL) return;

  for (unsigned long i = 0; i < num_samples; i++) {
    float t = (float)i / (float)SAMPLE_RATE;
    buffer[i] = (int16_t)(VOLUME * sin(2.0 * PI * frequency * t));
  }

  size_t bytes_written;
  i2s_channel_write(tx_handle, buffer, num_samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);

  free(buffer);
}

void setup() {
  Serial.begin(115200);
  initI2S();
  Serial.println("Playing Mozart...");
}

void loop() {
  int totalNotes = sizeof(melody) / sizeof(melody[0]);
  
  for (int i = 0; i < totalNotes; i++) {
    int noteDuration = 1000 / durations[i];
    playTone(melody[i], noteDuration);
    delay(noteDuration * 0.10); 
  }
  
  delay(3000);
}