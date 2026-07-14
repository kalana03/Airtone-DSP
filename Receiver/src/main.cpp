#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Bluetooth Music Receiver...");

  // 1. Configure the I2S Output pins to match your DAC wiring
  i2s_pin_config_t my_pin_config = {
    .bck_io_num = 26,       // BCK (Bit Clock)
    .ws_io_num = 25,        // LRCK (Word Select)
    .data_out_num = 22,     // DIN (Data In)
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  a2dp_sink.set_pin_config(my_pin_config);
  
  // 2. Start the Bluetooth receiver with a custom name
  a2dp_sink.start("ESP32_Music_Receiver"); 
  
  Serial.println("Ready! Connect your phone to 'ESP32_Music_Receiver' and play a song.");
}

void loop() {
  // The A2DP library handles all the heavy lifting and audio streaming in the background.
  // Leave the loop empty!
}