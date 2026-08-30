#include <Arduino.h>
#include <driver/i2s.h>
#include <speex/speex_preprocess.h> 

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define OVERDRIVE_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Speaker DAC Wiring
#define I2S_BCLK       26
#define I2S_LRC        25
#define I2S_DOUT       22

// Hardwired UART Pipeline
HardwareSerial MySerial(2); 
#define UART_RX_PIN 19 
#define UART_TX_PIN 23 

#define FRAME_SIZE 160  
#define SAMPLE_RATE 16000

int16_t incomingSamples[FRAME_SIZE];
int16_t speexFrame[FRAME_SIZE]; 

// ==========================================
// 🎛️ MASTER TUNING VARIABLES
// ==========================================

// 1. Gain Control (Volume & Headroom)
// 1.0f = Raw volume, 0.5f = Half volume (Best for Guitar), 2.0f = Double volume
float MANUAL_GAIN_MULTIPLIER = 5.0f; 

// 2. High-Pass Filter (Bass Cutoff)
// 0.984f = 40Hz (Guitar/Music), 0.962f = 100Hz (Voice/Hum elimination)
float HPF_ALPHA = 0.984f; 

// 3. Speex DSP Toggles (0 = Off, 1 = On)
int SPEEX_DENOISE_ON = 0; // Turn OFF for music, ON for voice
int SPEEX_NOISE_SUPPRESS_DB = 0; // How aggressive to cut background hiss

// 4. Noise Gate (Low Amplitude Cutoff)
float GATE_THRESHOLD = 400.0f; 
float ENV_SMOOTH = 0.998f;     

// 🎸 NEW: How slowly the volume fades out when the string gets quiet
// 0.999f = Very slow, natural fade. 0.990f = Fast, punchy fade.
float GATE_RELEASE_SPEED = 0.999f; 

// --- Noise Gate State Variables ---
float env_level = 0.0f;
float gate_multiplier = 2.0f; // This will act as our digital volume knob

// 5. Low-Pass Filter (High-frequency Hiss Killer)
// 1.0f = No filtering (bright), 0.6f = Moderate cut, 0.2f = Heavy cut (muffled)
float LPF_ALPHA = 0.6f; 
float lpf_y_prev = 0.0f;

// 6. Tube Overdrive Simulator
bool OVERDRIVE_ENABLED = true; // Set to false for a clean acoustic tone
float OVERDRIVE_DRIVE = 8.0f;  // 1.0 = Clean, 5.0 = Crunchy, 10.0+ = Heavy Metal Fuzz
float OVERDRIVE_OUTPUT_GAIN = 0.6f; // 1.0 = Full volume, 0.5 = Half volume, 0.2 = Low volume

// --- Overdrive State Variables ---
float od_pre1 = 0.0f;      // pre-filter state (one-pole)
float od_pre2 = 0.0f;      // second pre-filter state
float od_post1 = 0.0f;     // post-filter state
float od_post2 = 0.0f;     // second post-filter state
float od_dc_x = 0.0f;      // DC blocker input history
float od_dc_y = 0.0f;      // DC blocker output history
float od_drive_gain = 1.0f; // automatic gain compensation (auto‑calibrated)

// --- 7. Stadium Lead Delay (Echo) ---
bool DELAY_ENABLED = false;
const int DELAY_SAMPLES = 4000;       // 250ms echo at 16000Hz Sample Rate
float delay_buffer[DELAY_SAMPLES];    // The memory bank for the echo
int delay_index = 0;                  // Keeps track of where we are in the buffer

float DELAY_MIX = 0.4f;         // Volume of the echo (40%)
float DELAY_FEEDBACK = 0.3f;    // How long the echo repeats/trails off


// ==========================================


// --- Internal Filter State Variables ---
float med_z1 = 0.0f;
float med_z2 = 0.0f;

float dc_x_prev = 0.0f, dc_y_prev = 0.0f;
float DC_BLOCKER_R = 0.995f; 

float hpf_x_prev = 0.0f, hpf_y_prev = 0.0f;

SpeexPreprocessState *st;

// --- DSP Math Functions ---
float median_of_3(float a, float b, float c) {
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

float dc_block(float x, float *x_prev, float *y_prev, float R) {
    float y = x - *x_prev + R * (*y_prev);
    *x_prev = x;
    *y_prev = y;
    return y;
}

float high_pass(float x, float *x_prev, float *y_prev, float alpha) {
    float y = alpha * (*y_prev + x - *x_prev);
    *x_prev = x;
    *y_prev = y;
    return y;
}

void audio_denoise_init(void) {
    st = speex_preprocess_state_init(FRAME_SIZE, SAMPLE_RATE);

    // Apply the tuning toggles
    int denoise = SPEEX_DENOISE_ON;
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &denoise);

    int noise_suppress = SPEEX_NOISE_SUPPRESS_DB; 
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);

    // Keep AGC and VAD off for manual control stability
    int agc = 0; 
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC, &agc);
    int vad = 0; 
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_VAD, &vad);
}

void setupI2S() {
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
}

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        // Convert the incoming string (e.g., "15.5") into a float
        float newValue = atof(rxValue.c_str());
        
        // Update the DSP engine live!
        OVERDRIVE_DRIVE = newValue;
        
        Serial.print("📱 BLE Update! New Overdrive: ");
        Serial.println(OVERDRIVE_DRIVE);
      }
    }
};

void setup() {
  Serial.begin(115200);
  MySerial.setRxBufferSize(2048); 
  MySerial.begin(460800, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  
  setupI2S();
  audio_denoise_init(); 

  // 1. Name your device
  BLEDevice::init("Airtone DSP");
  
  // 2. Create the Server
  BLEServer *pServer = BLEDevice::createServer();
  
  // 3. Create the Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // 4. Create the Mailbox (Characteristic) for Overdrive
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         OVERDRIVE_CHAR_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  // Attach our listener to the mailbox
  pCharacteristic->setCallbacks(new MyCallbacks());

  // 5. Start broadcasting!
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("🎧 DSP Pipeline Active! Configured via Master Variables.");
}

void loop() {
  
  // The Elastic Flush: Dump old memory to kill latency
  if (MySerial.available() > 640) {
    while(MySerial.available()) {
      MySerial.read();
    }
  }

  if (MySerial.available() >= 2) {
    if (MySerial.read() == 0xAA) {
      if (MySerial.read() == 0xBB) {
        
        while(MySerial.available() < sizeof(incomingSamples)) {
           yield();
        }
        
        MySerial.readBytes((char*)incomingSamples, sizeof(incomingSamples));
        
        for(int i = 0; i < FRAME_SIZE; i++) {
          float raw = (float)incomingSamples[i];
          
          // 1. Median Filter
          float med_clean = median_of_3(raw, med_z1, med_z2);
          med_z2 = med_z1;
          med_z1 = raw; 
          
          // 2. DC Block
          float dc_clean = dc_block(med_clean, &dc_x_prev, &dc_y_prev, DC_BLOCKER_R);
          
          // 3. High-Pass Filter (Kills low rumble)
          float hpf_clean = high_pass(dc_clean, &hpf_x_prev, &hpf_y_prev, HPF_ALPHA);
          
          // --- NEW: 3.5 Low-Pass Filter (Kills high static/hiss) ---
          float lpf_clean = (LPF_ALPHA * hpf_clean) + ((1.0f - LPF_ALPHA) * lpf_y_prev);
          lpf_y_prev = lpf_clean;
          
          // 4. Manual Gain & Headroom (Your Pre-Amp)
          float final_audio = lpf_clean * MANUAL_GAIN_MULTIPLIER;
          
          // --- 5. THE SMOOTH NOISE GATE ---
          env_level = (ENV_SMOOTH * env_level) + ((1.0f - ENV_SMOOTH) * abs(final_audio));
          
          if (env_level < GATE_THRESHOLD) {
              // The room is quiet! Slowly fade out the volume (Release)
              gate_multiplier = gate_multiplier * GATE_RELEASE_SPEED; 
          } else {
              // You strummed! Instantly snap the volume back to 100% (Attack)
              gate_multiplier = 1.0f; 
          }
          
          // Anti-click protection: If the volume gets insanely tiny, just clamp it to zero
          if (gate_multiplier < 0.001f) gate_multiplier = 0.0f;
          
          // Apply the smooth digital volume knob to the audio
          final_audio = final_audio * gate_multiplier;
          // --------------------------------
          
          // --- 6. TUBE OVERDRIVE (The Screaming Distortion) ---
          if (OVERDRIVE_ENABLED && final_audio != 0.0f) {
            // Normalize to -1..1
            float x = final_audio / 32767.0f;

            // --- Pre‑filter (gentle low-pass to calm buzzy pre‑distortion) ---
            // 6 kHz cutoff (16000 Hz sample rate)
            float a_pre = 0.35f;  // ≈ 1 - exp(-2π*6000/16000)
            od_pre1 = od_pre1 + a_pre * (x - od_pre1);
            od_pre2 = od_pre2 + a_pre * (od_pre1 - od_pre2);
            float filtered = od_pre2;

            // --- Drive (with soft clipping) ---
            // We use a drive that saturates at ±4.0 for smooth, musical distortion
            float driven = filtered * OVERDRIVE_DRIVE;
            // Clamp to prevent extreme values that cause aliasing
            if (driven > 4.0f) driven = 4.0f;
            if (driven < -4.0f) driven = -4.0f;

            // --- Asymmetric tube-style waveshaping ---
            float bias = 0.2f;
            float shaped = tanhf(driven + bias) - tanhf(bias);

            // --- DC removal (the bias creates DC) ---
            float dc_blocked = shaped - od_dc_x + 0.995f * od_dc_y;
            od_dc_x = shaped;
            od_dc_y = dc_blocked;

            // --- Post‑filter (cut harsh high frequencies, anti‑alias) ---
            float a_post = 0.25f;   // ≈ 1 - exp(-2π*8000/16000)
            od_post1 = od_post1 + a_post * (dc_blocked - od_post1);
            od_post2 = od_post2 + a_post * (od_post1 - od_post2);
            float saturated = od_post2;

            // --- Wet/dry mix (clean blend) ---
            float mix = 0.7f;       // 0.7 = 70% saturated, 30% clean
            float mixed = filtered * (1.0f - mix) + saturated * mix;

            // --- Auto‑gain compensation (keeps amplitude constant) ---
            // This simple formula gives unity gain when drive ≈ 1.0,
            // and gradually reduces gain at higher drive to prevent clipping.
            od_drive_gain = 1.0f / (1.0f + 0.15f * OVERDRIVE_DRIVE * OVERDRIVE_DRIVE);
            mixed *= od_drive_gain;

            // --- Final soft safety limiter (prevents digital clipping) ---
            if (mixed > 1.0f) mixed = 1.0f;
            if (mixed < -1.0f) mixed = -1.0f;

            // --- Apply output gain user wants ---
            final_audio = mixed * OVERDRIVE_OUTPUT_GAIN * 32767.0f;
        }

          
          // --- 🎸 NEW: 7. STADIUM DELAY (The Solo Echo) ---
          if (DELAY_ENABLED) {
              // Read what we played 250ms ago
              float echo_audio = delay_buffer[delay_index];
              
              // Mix the live distortion with the echo
              float stadium_sound = final_audio + (echo_audio * DELAY_MIX);
              
              // Save the current sound back into memory so it repeats later (Feedback)
              delay_buffer[delay_index] = final_audio + (echo_audio * DELAY_FEEDBACK);
              
              // Move the playhead forward (looping back to 0 if it hits the end)
              delay_index++;
              if (delay_index >= DELAY_SAMPLES) {
                  delay_index = 0;
              }
              
              final_audio = stadium_sound;
          }
          // ------------------------------------------------

          // 8. Hard clipping protector (Safety net)
          if (final_audio > 32767.0f) final_audio = 32767.0f;
          if (final_audio < -32768.0f) final_audio = -32768.0f;
          
          speexFrame[i] = (int16_t)final_audio;
        }
        
        // 5. Speex Engine
        speex_preprocess_run(st, speexFrame); 
        
        // 6. Output to Speaker
        size_t bytes_written;
        i2s_write(I2S_NUM_0, speexFrame, sizeof(speexFrame), &bytes_written, 10);

        // Send to Teleplot
        Serial.print(">WiredAudio:");
        Serial.println(speexFrame[0]); 
      }
    }
  }
}