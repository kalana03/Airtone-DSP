#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <driver/i2s.h>

#define I2S_BCLK       26
#define I2S_LRC        25
#define I2S_DOUT       22

#define BUFFER_SIZE 64
typedef struct struct_message {
    int16_t audioSamples[BUFFER_SIZE];
} struct_message;

struct_message audioData;

float filteredSample = 0;
float alpha = 0.15; 

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 10000, 
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&audioData, incomingData, sizeof(audioData));
  
  // Create a temporary array to hold the math-corrected audio
  int16_t outBuffer[BUFFER_SIZE];
  
  // Process the entire chunk
  for(int i = 0; i < BUFFER_SIZE; i++) {
    filteredSample = (alpha * audioData.audioSamples[i]) + ((1.0 - alpha) * filteredSample);
    outBuffer[i] = ((int)filteredSample - 2048) << 3;
  }

  // Write the whole chunk to the DAC at once
  size_t bytes_written;
  i2s_write(I2S_NUM_0, outBuffer, sizeof(outBuffer), &bytes_written, 0);
}

void setup() {
  Serial.begin(115200);
  setupI2S();
  
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Empty loop
}