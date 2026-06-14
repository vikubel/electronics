/*
  ============================================================
  ESP32-S3 Mini  +  MAX98357A I2S Amplifier  +  16Ω/25W Speaker
  ============================================================
  Streams an 8kHz / 8-bit unsigned PCM "Hello World" sample
  in a loop, using the ESP-IDF v5.x "i2s_std" driver
  (Arduino Core 3.x compatible).

  Wiring (matches your setup):
    MAX98357A Vin  -> ESP32 5V
    MAX98357A GND  -> ESP32 GND
    MAX98357A BCLK -> GPIO 7
    MAX98357A LRC  -> GPIO 8
    MAX98357A DIN  -> GPIO 9
    MAX98357A GAIN -> GND   (12dB hardware gain)
  ============================================================
*/

#include <Arduino.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"

// ---------------- Pin Definitions ----------------
#define I2S_BCLK_PIN   GPIO_NUM_7
#define I2S_LRC_PIN    GPIO_NUM_8
#define I2S_DOUT_PIN   GPIO_NUM_9

#define SAMPLE_RATE    8000

// ---------------- Amplification ----------------
// Incoming samples are 8-bit unsigned, centered at 128 (silence = 128).
// After subtracting 128 the value range is -128..127.
// Multiplying by 256 maps this almost exactly onto the full int16_t
// range (-32768 .. +32512). We then explicitly clamp to guarantee we
// never wrap around / overflow, even if a sample is exactly 127.
#define AMP_SHIFT 8   // multiply by 2^8 = 256 -> max possible gain for 8->16 bit expansion

// ============================================================
// PLACEHOLDER AUDIO DATA (8kHz, 8-bit unsigned PCM)
// ============================================================
// This is a short synthetic "two-syllable" tone pattern (NOT real
// speech) so the sketch compiles and plays something audible out
// of the box. Replace this array with a real "Hello World" recording
// converted to 8kHz/8-bit unsigned PCM (see conversion notes at the
// bottom of this file).
const uint8_t audioData[] PROGMEM = {
  // --- "Hello" syllable (64 samples) ---
  128,130,137,145,153,157,155,145,128,107, 83, 65, 53, 53, 66, 92,
  128,166,199,220,228,220,199,166,128, 90, 57, 36, 28, 36, 57, 90,
  128,166,199,220,228,220,199,166,128, 90, 57, 36, 28, 36, 57, 90,
  128,163,190,202,201,190,171,148,128,113,105,103,108,116,123,128,

  // --- short silence gap (16 samples) ---
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,

  // --- "World" syllable (64 samples) ---
  128,130,137,145,153,157,155,145,128,107, 83, 65, 53, 53, 66, 92,
  128,166,199,220,228,220,199,166,128, 90, 57, 36, 28, 36, 57, 90,
  128,166,199,220,228,220,199,166,128, 90, 57, 36, 28, 36, 57, 90,
  128,163,190,202,201,190,171,148,128,113,105,103,108,116,123,128,

  // --- trailing silence before loop repeats (32 samples) ---
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

const size_t audioDataLen = sizeof(audioData);

// ============================================================
// I2S channel handle (new driver/i2s_std.h API)
// ============================================================
i2s_chan_handle_t tx_handle = NULL;

void setupI2S() {
  // ---- 1. Channel configuration ----
  // I2S_CHANNEL_DEFAULT_CONFIG sets reasonable defaults for a master
  // TX channel. We then tweak the DMA buffer parameters:
  //
  //   dma_desc_num  = number of DMA descriptors (buffers) in the ring
  //   dma_frame_num = number of audio frames per DMA buffer
  //
  // More/larger buffers = more headroom against stuttering, at the
  // cost of slightly more RAM and latency. 8 buffers x 256 frames is
  // a good, stable setting for an 8kHz mono/stereo stream.
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num  = 8;
  chan_cfg.dma_frame_num = 256;
  chan_cfg.auto_clear    = true;  // auto-fill silence if buffer underruns (prevents garbage noise)

  esp_err_t err = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
  if (err != ESP_OK) {
    Serial.printf("i2s_new_channel failed: %d\n", err);
    while (true) delay(1000);
  }

  // ---- 2. Standard (Philips) I2S mode configuration ----
  // MAX98357A expects standard I2S (Philips) format. We configure it
  // for 16-bit samples in STEREO mode and duplicate the same sample
  // into the Left and Right slots. This is the most "amp-agnostic"
  // configuration: regardless of how the MAX98357A's internal SD pin
  // is biased (mono-left / mono-average), both channels carry valid,
  // identical audio, so the amp is guaranteed to receive a signal.
  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                     I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,     // MAX98357A does not need MCLK
      .bclk = I2S_BCLK_PIN,
      .ws   = I2S_LRC_PIN,
      .dout = I2S_DOUT_PIN,
      .din  = I2S_GPIO_UNUSED,     // TX only, no microphone input
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };

  err = i2s_channel_init_std_mode(tx_handle, &std_cfg);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_init_std_mode failed: %d\n", err);
    while (true) delay(1000);
  }

  // ---- 3. Enable the channel ----
  // This starts the BCLK/WS clocks and arms the DMA. From this point
  // the I2S peripheral is actively clocking the MAX98357A, even
  // before we write data (auto_clear keeps the line quiet/silent).
  err = i2s_channel_enable(tx_handle);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_enable failed: %d\n", err);
    while (true) delay(1000);
  }

  Serial.println("I2S initialized successfully.");
}

// ============================================================
// Convert + amplify + stream the PCM buffer
// ============================================================
void playAudioBuffer() {
  // Process audio in small chunks so we don't need a giant RAM
  // buffer, and so i2s_channel_write() can feed the DMA ring
  // continuously (each write blocks until DMA has room, which
  // naturally paces playback at the correct sample rate).
  const size_t CHUNK_SAMPLES = 256;
  static int16_t stereoBuffer[CHUNK_SAMPLES * 2]; // interleaved L,R

  size_t idx = 0;
  while (idx < audioDataLen) {
    size_t samplesToProcess = min(CHUNK_SAMPLES, audioDataLen - idx);

    for (size_t i = 0; i < samplesToProcess; i++) {
      // 1. Read the raw 8-bit unsigned sample from PROGMEM
      uint8_t raw = pgm_read_byte(&audioData[idx + i]);

      // 2. Center it around 0 (silence = 128 -> 0)
      //    Resulting range: -128 .. +127
      int16_t centered = (int16_t)raw - 128;

      // 3. Aggressively scale up to (near) full int16 range.
      //    Use int32_t for the multiply to avoid any intermediate
      //    overflow before clamping.
      int32_t amplified = (int32_t)centered << AMP_SHIFT; // *256

      // 4. Hard clamp to int16_t limits (defensive - protects
      //    against any future change to AMP_SHIFT or input range).
      if (amplified > 32767)  amplified = 32767;
      if (amplified < -32768) amplified = -32768;

      int16_t sample = (int16_t)amplified;

      // 5. Duplicate into both Left and Right slots so the
      //    MAX98357A receives valid data regardless of its
      //    internal channel-select state.
      stereoBuffer[i * 2]     = sample; // Left
      stereoBuffer[i * 2 + 1] = sample; // Right
    }

    // 6. Blocking write into the I2S DMA ring buffer.
    //    portMAX_DELAY means this call waits until there's room,
    //    which automatically throttles our loop to the real
    //    8kHz playback rate -> smooth, glitch-free audio with
    //    no manual delay() needed.
    size_t bytesWritten = 0;
    i2s_channel_write(tx_handle,
                       stereoBuffer,
                       samplesToProcess * 2 * sizeof(int16_t),
                       &bytesWritten,
                       portMAX_DELAY);

    idx += samplesToProcess;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting I2S audio demo...");
  setupI2S();
}

void loop() {
  playAudioBuffer();
  delay(400); // brief pause between loop repeats
}