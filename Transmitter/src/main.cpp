#include <Arduino.h>

#define PIEZO_PIN 1

void setup() {
    Serial.begin(115200);
    analogReadResolution(12);
    
    // Wait for ADC to settle
    delay(500);
    
    Serial.println("Finding your ADC center...");
    Serial.println("DO NOT touch or pluck the guitar");
    Serial.println("Reading 2000 samples...");
    
    long sum = 0;
    int minVal = 4095;
    int maxVal = 0;
    
    for (int i = 0; i < 2000; i++) {
        int raw = analogRead(PIEZO_PIN);
        sum += raw;
        if (raw < minVal) minVal = raw;
        if (raw > maxVal) maxVal = raw;
        delayMicroseconds(100);
    }
    
    long center = sum / 2000;
    
    Serial.println("─────────────────────────────");
    Serial.print("Your ADC center = ");
    Serial.println(center);
    Serial.print("Min value seen  = ");
    Serial.println(minVal);
    Serial.print("Max value seen  = ");
    Serial.println(maxVal);
    Serial.print("Noise floor     = ");
    Serial.println(maxVal - minVal);
    Serial.println("─────────────────────────────");
    Serial.println("Copy the center number and tell me what it is.");
}

void loop() {
    // Nothing
}