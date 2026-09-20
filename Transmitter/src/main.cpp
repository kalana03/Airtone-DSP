#include <Arduino.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h> // Added for Wi-Fi channel locking

#define I2S_WS 4   
#define I2S_SCK 5  
#define I2S_SD 6   

#define BUFFER_SIZE 160 

// 🚨 REPLACE THIS WITH YOUR RECEIVER'S MAC ADDRESS 🚨
uint8_t receiverAddress[] = {0x70, 0x4B, 0xCA, 0x90, 0xE2, 0x50};

// The "Pizza Box": Holds half the audio (80 samples = 160 bytes) + an ID tag
typedef struct struct_message {
  uint8_t packet_id; // 0 for the first half, 1 for the second half
  int16_t audioSamples[80];
} struct_message;

struct_message audioPacket;
esp_now_peer_info_t peerInfo;

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
  Serial.begin(115200);
  setupI2S();

  // Start Wi-Fi in Station Mode and explicitly lock to Channel 1
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register the receiver
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 1;  // Updated to match the locked Wi-Fi channel
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  int32_t rawI2SData[BUFFER_SIZE];
  size_t bytesIn = 0;
  esp_err_t result;
  static int drop_counter = 0;
  
  i2s_read(I2S_NUM_0, &rawI2SData, sizeof(rawI2SData), &bytesIn, portMAX_DELAY);
  
  drop_counter++;

  // 📦 PACKET 0 (The Target for the 5% Drop)
  audioPacket.packet_id = 0;
for(int i = 0; i < 80; i++) {
  audioPacket.audioSamples[i] = rawI2SData[i] >> 14; 
}
result = esp_now_send(receiverAddress, (uint8_t *) &audioPacket, sizeof(audioPacket));
if (result != ESP_OK) delay(1);
  // 📦 PACKET 1 (Always sent so the receiver triggers the check)
  audioPacket.packet_id = 1;
  for(int i = 0; i < 80; i++) {
    audioPacket.audioSamples[i] = rawI2SData[i + 80] >> 14; 
  }
  result = esp_now_send(receiverAddress, (uint8_t *) &audioPacket, sizeof(audioPacket));
  if (result != ESP_OK) delay(1); 
}