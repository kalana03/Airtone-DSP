#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Your Receiver's MAC Address
uint8_t broadcastAddress[] = {0x70, 0x4B, 0xCA, 0x90, 0xE2, 0x50};

#define BUFFER_SIZE 64
typedef struct struct_message {
    int16_t audioSamples[BUFFER_SIZE];
} struct_message;

struct_message audioData;
esp_now_peer_info_t peerInfo;
const int piezoPin = 1; 

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  // Capture 64 samples at ~16kHz
  for(int i = 0; i < BUFFER_SIZE; i++) {
    audioData.audioSamples[i] = analogRead(piezoPin);
    // 32us delay + 30us ADC read time = ~62.5us total (16,000 Hz)
    delayMicroseconds(32); 
  }
  
  // Blast the packet over the air
  esp_now_send(broadcastAddress, (uint8_t *) &audioData, sizeof(audioData));
}