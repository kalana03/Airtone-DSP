#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_WS 4   
#define I2S_SCK 5  
#define I2S_SD 6   

HardwareSerial MySerial(1); 
#define UART_TX_PIN 7 
#define UART_RX_PIN 8 

// 🚨 MUST BE 160 to match the Receiver's Speex engine!
#define BUFFER_SIZE 160 
int16_t audioSamples[BUFFER_SIZE];

const uint8_t syncWord[2] = {0xAA, 0xBB};

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000, 
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, 
    .data_in_num = I2S_SD              
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void setup() {
  MySerial.begin(460800, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  setupI2S();
}

void loop() {
  int32_t rawI2SData[BUFFER_SIZE];
  size_t bytesIn = 0;
  
  i2s_read(I2S_NUM_0, &rawI2SData, sizeof(rawI2SData), &bytesIn, portMAX_DELAY);
  
  for(int i = 0; i < BUFFER_SIZE; i++) {
    audioSamples[i] = rawI2SData[i] >> 14; 
  }
  
  // Blast the Sync Word, then exactly 320 bytes of audio
  MySerial.write(syncWord, 2);
  MySerial.write((uint8_t *)audioSamples, sizeof(audioSamples));
}