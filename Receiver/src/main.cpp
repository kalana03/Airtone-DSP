#include <Arduino.h>
#include <driver/i2s.h>
#include <speex/speex_preprocess.h> 
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ==========================================
// 🔌 HARDWARE WIRING & PINS
// ==========================================
#define I2S_BCLK       26
#define I2S_LRC        25
#define I2S_DOUT       22

#define BYPASS_SWITCH_PIN 32
#define LED_SWITCH_PIN    12  // Red
#define LED_BLE_PIN       14  // Blue
#define LED_ESPNOW_PIN    27  // Green

// ==========================================
// 📻 BLUETOOTH CONFIG
// ==========================================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define OVERDRIVE_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ==========================================
// 🎛️ MASTER TUNING VARIABLES
// ==========================================
float MANUAL_GAIN_MULTIPLIER = 1.5f; 
float HPF_ALPHA = 0.984f; 
float LPF_ALPHA = 0.6f; 

int SPEEX_DENOISE_ON = 0; 
int SPEEX_NOISE_SUPPRESS_DB = -15; 

float GATE_THRESHOLD = 400.0f; 
float ENV_SMOOTH = 0.998f;     
float GATE_RELEASE_SPEED = 0.999f; 

bool OVERDRIVE_ENABLED = true; 
float OVERDRIVE_DRIVE = 8.0f;  
float OVERDRIVE_OUTPUT_GAIN = 0.6f; 

bool DELAY_ENABLED = false;
float DELAY_MIX = 0.4f;         
float DELAY_FEEDBACK = 0.3f;    

// ==========================================
// 🧠 INTERNAL STATE & MEMORY
// ==========================================
#define FRAME_SIZE 160  
#define SAMPLE_RATE 16000
const int DELAY_SAMPLES = 4000;       

bool take_snapshot = false;
float snapshot_raw[160];
float snapshot_processed[160];

int16_t incomingSamples[FRAME_SIZE];
int16_t speexFrame[FRAME_SIZE]; 
float delay_buffer[DELAY_SAMPLES];    

int delay_index = 0;                  
float env_level = 0.0f;
float gate_multiplier = 2.0f; 
float lpf_y_prev = 0.0f;
float od_pre1 = 0.0f, od_pre2 = 0.0f, od_post1 = 0.0f, od_post2 = 0.0f;
float od_dc_x = 0.0f, od_dc_y = 0.0f, od_drive_gain = 1.0f; 
float med_z1 = 0.0f, med_z2 = 0.0f;
float dc_x_prev = 0.0f, dc_y_prev = 0.0f, DC_BLOCKER_R = 0.995f; 
float hpf_x_prev = 0.0f, hpf_y_prev = 0.0f;

bool hardware_bypass = false;
unsigned long lastPacketTime = 0;
volatile bool frame_ready = false;
volatile int debug_packet_count = 0;
SpeexPreprocessState *st;

int16_t last_good_packet[80];
unsigned long total_lpr_time = 0;
unsigned int lpr_trigger_count = 0;
volatile bool packet_0_received = false;

// ==========================================
// 📡 ESP-NOW WIRELESS PROTOCOL
// ==========================================
typedef struct struct_message {
  uint8_t packet_id;
  int16_t audioSamples[80];
} struct_message;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  debug_packet_count++; 
  
  struct_message packet;
  memcpy(&packet, incomingData, sizeof(packet));

  if (packet.packet_id == 0) {
    memcpy(&incomingSamples[0], packet.audioSamples, 80 * sizeof(int16_t));
    memcpy(&last_good_packet[0], packet.audioSamples, 80 * sizeof(int16_t)); // 💾 Save good state
    packet_0_received = true; // Mark as successfully received
  } 
  else if (packet.packet_id == 1) {
    
    // 🚨 DETECT DROPPED PACKET 
    if (!packet_0_received) {
       
       unsigned long lpr_start = micros(); // ⏱️ Start Stopwatch
       
       // 🛠️ Execute PLC: Copy last known good data into the missing slot
       memcpy(&incomingSamples[0], last_good_packet, 80 * sizeof(int16_t));
       
       unsigned long lpr_time = micros() - lpr_start; // ⏱️ Stop Stopwatch
       
       // 📊 Calculate and Print the Average
       total_lpr_time += lpr_time;
       lpr_trigger_count++;
       unsigned long lpr_avg = total_lpr_time / lpr_trigger_count;
       
       Serial.printf("LPR Triggered! Time: %lu us | RUNNING AVG: %lu us\n", lpr_time, lpr_avg);
    }
    
    memcpy(&incomingSamples[80], packet.audioSamples, 80 * sizeof(int16_t));
    frame_ready = true; 
    packet_0_received = false; // Reset flag for the next frame
  }
}

// ==========================================
// 📱 BLUETOOTH LISTENERS
// ==========================================
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("📱 Bluetooth Connected!");
      digitalWrite(LED_BLE_PIN, HIGH); 
    };
    void onDisconnect(BLEServer* pServer) {
      Serial.println("📱 Bluetooth Disconnected!");
      digitalWrite(LED_BLE_PIN, LOW); 
      pServer->startAdvertising();    
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() >= 3) {
        char tag = rxValue[0];
        float newValue = atof(rxValue.c_str() + 2);
        
        Serial.printf("📱 BLE Update! Tag: [%c] | Value: %.3f\n", tag, newValue);

        switch (tag) {
          case 'G': MANUAL_GAIN_MULTIPLIER = newValue; break;
          case 'B': DC_BLOCKER_R = newValue; break;
          case 'H': HPF_ALPHA = newValue; break;
          case 'L': LPF_ALPHA = newValue; break;
          case 'T': GATE_THRESHOLD = newValue; break;
          case 'E': ENV_SMOOTH = newValue; break;
          case 'R': GATE_RELEASE_SPEED = newValue; break;
          case 'N': 
            SPEEX_DENOISE_ON = (int)newValue; 
            speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &SPEEX_DENOISE_ON);
            break;
          case 'S': 
            SPEEX_NOISE_SUPPRESS_DB = (int)newValue; 
            speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &SPEEX_NOISE_SUPPRESS_DB);
            break;
          case 'O': OVERDRIVE_ENABLED = (newValue > 0.5f); break; 
          case 'D': OVERDRIVE_DRIVE = newValue; break;
          case 'V': OVERDRIVE_OUTPUT_GAIN = newValue; break;
          case 'Y': DELAY_ENABLED = (newValue > 0.5f); break;
          case 'M': DELAY_MIX = newValue; break;
          case 'F': DELAY_FEEDBACK = newValue; break;
        }
      }
    }
};

// ==========================================
// 🧮 DSP MATH HELPERS
// ==========================================
float median_of_3(float a, float b, float c) {
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}
float dc_block(float x, float *x_prev, float *y_prev, float R) {
    float y = x - *x_prev + R * (*y_prev);
    *x_prev = x; *y_prev = y; return y;
}
float high_pass(float x, float *x_prev, float *y_prev, float alpha) {
    float y = alpha * (*y_prev + x - *x_prev);
    *x_prev = x; *y_prev = y; return y;
}

// ==========================================
// 🚀 SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // 1. Hardware Pins
  pinMode(BYPASS_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_SWITCH_PIN, OUTPUT);
  pinMode(LED_BLE_PIN, OUTPUT);
  pinMode(LED_ESPNOW_PIN, OUTPUT);

  // 2. I2S Audio
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE, 
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4, 
    .dma_buf_len = FRAME_SIZE,
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

  // 3. Speex Engine Init
  st = speex_preprocess_state_init(FRAME_SIZE, SAMPLE_RATE);
  speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &SPEEX_DENOISE_ON);
  speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &SPEEX_NOISE_SUPPRESS_DB);
  int off = 0; 
  speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC, &off);
  speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_VAD, &off);

  // 4. ESP-NOW Init
  WiFi.mode(WIFI_STA);

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); 
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW Init Failed");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  // 5. BLE Init
  BLEDevice::init("Airtone DSP");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pChar = pService->createCharacteristic(
    OVERDRIVE_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pChar->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("🎸 Airtone DSP Receiver Ready!");
}

// ==========================================
// 🔁 MAIN AUDIO LOOP
// ==========================================
void loop() {

  // 📸 4-SECOND SNAPSHOT TIMER
  static unsigned long lastSnapshotTime = 0; 
  if (millis() - lastSnapshotTime > 4000) { 
    take_snapshot = true; 
    lastSnapshotTime = millis();
  }

  // 🐛 2-SECOND SYSTEM STATUS MONITOR
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 2000) {
    Serial.println("--- 🐛 SYSTEM STATUS ---");
    Serial.printf("BLE Pin (14) State: %s\n", digitalRead(LED_BLE_PIN) ? "HIGH (ON)" : "LOW (OFF)");
    Serial.printf("ESP-NOW Packets Received (last 2s): %d\n", debug_packet_count);
    
    // Reset counter for the next 2 seconds
    debug_packet_count = 0; 
    lastDebugTime = millis();
  }

  // 🚨 MOVED OUTSIDE: Always check the switch, even if wireless audio is paused!
  //hardware_bypass = (digitalRead(BYPASS_SWITCH_PIN) == LOW);
    hardware_bypass = false; 
  digitalWrite(LED_SWITCH_PIN, HIGH);
  digitalWrite(LED_SWITCH_PIN, hardware_bypass ? HIGH : LOW);

  if (frame_ready) {
    frame_ready = false; 

    // Visual Feedback: Data arrived!
    digitalWrite(LED_ESPNOW_PIN, HIGH);
    lastPacketTime = millis();

    // ⏱️ Start Stopwatch for Python Plot
    unsigned long start_dsp_timer = 0;
    if (take_snapshot) start_dsp_timer = micros(); 

    for(int i = 0; i < FRAME_SIZE; i++) {
      
      float raw = (float)incomingSamples[i];
      
      // 🚪 TRUE BYPASS
      // 🚪 TRUE BYPASS
      if (hardware_bypass) {
         if (take_snapshot) {
            snapshot_raw[i] = raw;
            snapshot_processed[i] = raw; // In bypass, output equals input!
         }
         speexFrame[i] = (int16_t)raw;
         continue; 
      }
      
      // 1. Median & DC Block
      float med_clean = median_of_3(raw, med_z1, med_z2);
      med_z2 = med_z1; med_z1 = raw; 
      float dc_clean = dc_block(med_clean, &dc_x_prev, &dc_y_prev, DC_BLOCKER_R);
      
      // 2. EQ Filters
      float hpf_clean = high_pass(dc_clean, &hpf_x_prev, &hpf_y_prev, HPF_ALPHA);
      float lpf_clean = (LPF_ALPHA * hpf_clean) + ((1.0f - LPF_ALPHA) * lpf_y_prev);
      lpf_y_prev = lpf_clean;
      
      // 3. Pre-Amp Gain
      float final_audio = lpf_clean * MANUAL_GAIN_MULTIPLIER;
      
      // 4. Noise Gate
      env_level = (ENV_SMOOTH * env_level) + ((1.0f - ENV_SMOOTH) * abs(final_audio));
      if (env_level < GATE_THRESHOLD) gate_multiplier *= GATE_RELEASE_SPEED; 
      else gate_multiplier = 1.0f; 
      if (gate_multiplier < 0.001f) gate_multiplier = 0.0f;
      final_audio *= gate_multiplier;
      
      // 5. Tube Overdrive
      if (OVERDRIVE_ENABLED && final_audio != 0.0f) {
        float x = final_audio / 32767.0f;
        float a_pre = 0.35f;  
        od_pre1 = od_pre1 + a_pre * (x - od_pre1); od_pre2 = od_pre2 + a_pre * (od_pre1 - od_pre2);
        
        float driven = od_pre2 * OVERDRIVE_DRIVE;
        if (driven > 4.0f) driven = 4.0f; else if (driven < -4.0f) driven = -4.0f;
        
        float bias = 0.2f;
        float shaped = tanhf(driven + bias) - tanhf(bias);
        
        float dc_blocked = shaped - od_dc_x + 0.995f * od_dc_y;
        od_dc_x = shaped; od_dc_y = dc_blocked;
        
        float a_post = 0.25f;   
        od_post1 = od_post1 + a_post * (dc_blocked - od_post1); od_post2 = od_post2 + a_post * (od_post1 - od_post2);
        
        float mixed = (od_pre2 * 0.3f) + (od_post2 * 0.7f);
        od_drive_gain = 1.0f / (1.0f + 0.15f * OVERDRIVE_DRIVE * OVERDRIVE_DRIVE);
        mixed *= od_drive_gain;
        
        if (mixed > 1.0f) mixed = 1.0f; else if (mixed < -1.0f) mixed = -1.0f;
        final_audio = mixed * OVERDRIVE_OUTPUT_GAIN * 32767.0f;
      }
      
      // 6. Stadium Delay
      if (DELAY_ENABLED) {
          float echo_audio = delay_buffer[delay_index];
          float stadium_sound = final_audio + (echo_audio * DELAY_MIX);
          delay_buffer[delay_index] = final_audio + (echo_audio * DELAY_FEEDBACK);
          delay_index++;
          if (delay_index >= DELAY_SAMPLES) delay_index = 0;
          final_audio = stadium_sound;
      }

      // 7. Master Soft Limiter
      float head_room = final_audio / 24000.0f; 
      final_audio = 24000.0f * tanhf(head_room);

      if (take_snapshot) {
          snapshot_raw[i] = raw;
          snapshot_processed[i] = final_audio;
      }

      speexFrame[i] = (int16_t)final_audio;
    }

    // 📸 Print the Snapshot with the Timer Data!
    if (take_snapshot) {
        unsigned long dsp_duration = micros() - start_dsp_timer; // ⏱️ Stop Stopwatch
        take_snapshot = false; // Reset trigger
        
        Serial.println("START_PLOT");
        Serial.print("TIME:");
        Serial.println(dsp_duration);
        
        for(int j = 0; j < FRAME_SIZE; j++) {
            Serial.print(snapshot_raw[j]);
            Serial.print(",");
            Serial.println(snapshot_processed[j]);
        }
        Serial.println("END_PLOT");
    }
    
    // 8. Speex AI & DAC Output
    speex_preprocess_run(st, speexFrame); 
    size_t bytes_written;
    i2s_write(I2S_NUM_0, speexFrame, sizeof(speexFrame), &bytes_written, 10);
  }

  // 9. ESP-NOW LED Timeout (Turns off Green LED if transmitter stops)
  if (millis() - lastPacketTime > 100) {
    digitalWrite(LED_ESPNOW_PIN, LOW);
  }
}