#include <Arduino.h>

const int piezoPin = 1; 

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); 
}

void loop() {
  int audioSample = analogRead(piezoPin);
  
  // Teleplot specific format: >Name:Value
  Serial.print(">AudioWave:");
  Serial.println(audioSample);
  
  delay(2); 
}