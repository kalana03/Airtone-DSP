#include <Arduino.h>
#include <math.h>
#include <driver/i2s.h>
#include <speex/speex_preprocess.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ==========================================
// HARDWARE WIRING & PINS
// ==========================================
#define I2S_BCLK       26
#define I2S_LRC        25
#define I2S_DOUT       22

#define BYPASS_SWITCH_PIN 32
#define LED_SWITCH_PIN    12  // Red
#define LED_BLE_PIN       14  // Blue
#define LED_ESPNOW_PIN    27  // Green

// ==========================================
// BLUETOOTH CONFIG
// ==========================================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define OVERDRIVE_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ==========================================
// CONSTANTS
// ==========================================
#define FRAME_SIZE   160
#define SAMPLE_RATE  16000

// Pitch detector geometry (NSDF / McLeod method)
#define PITCH_W       384                      // correlation window (samples)
#define PITCH_MAXLAG  232                      // ~69 Hz lowest (below guitar low E 82 Hz)
#define PITCH_MINLAG  16                       // 1000 Hz highest
#define PITCH_BUF     (PITCH_W + PITCH_MAXLAG + 24)   // history length (640)

// ==========================================
// MASTER TUNING VARIABLES
// ==========================================
float MANUAL_GAIN_MULTIPLIER = 3.0f;   // G: clean boost
float DC_BLOCKER_R = 0.995f;           // B
float HPF_ALPHA = 0.984f;              // H
float LPF_ALPHA = 0.85f;               // L: ~4.8 kHz; 1.0 = off
bool  MEDIAN_ENABLED = false;          // K

float GATE_THRESHOLD = 120.0f;         // T: 0 = gate off
float ENV_SMOOTH = 0.995f;             // E
float GATE_ATTACK = 0.02f;             // A
float GATE_RELEASE_SPEED = 0.9995f;    // R: slow, natural decay

bool  OVERDRIVE_ENABLED = false;       // O
float OVERDRIVE_DRIVE = 8.0f;          // D
float OVERDRIVE_OUTPUT_GAIN = 0.6f;    // V

bool  DELAY_ENABLED = false;           // Y
float DELAY_MIX = 0.4f;                // M
float DELAY_FEEDBACK = 0.3f;           // F

bool SNAPSHOT_ENABLED = false;         // serial plot off by default

int SPEEX_DENOISE_ON = 0;              // N
int SPEEX_NOISE_SUPPRESS_DB = -25;     // S (always forced negative)

// ---- SYNTHETIC SOUND (pitch-tracking synth) ----
bool  SYNTH_ENABLED = false;           // Z: master on/off for the synthetic wave
int   SYNTH_WAVE = 0;                  // W: 0 sine, 1 triangle, 2 saw, 3 square, 4 organ
float SYNTH_MIX = 1.0f;                // X: 0 = guitar only, 1 = synth only
float SYNTH_LEVEL = 1.0f;              // I: synth loudness trim
float SYNTH_OCTAVE_MULT = 1.0f;        // Q: -2..2 octaves (stored as multiplier)
bool  SYNTH_SNAP_TO_NOTE = true;       // P: snap to nearest semitone
float SYNTH_GLIDE = 0.01f;             // U: per-sample glide coefficient (0.0005 slow .. 1 instant)
float SYNTH_CLARITY = 0.75f;           // C: min pitch clarity 0..1 to accept a note
float SYNTH_MIN_RMS = 60.0f;           // J: min input RMS to attempt detection
float SYNTH_RELEASE = 0.0015f;         // envelope release per sample

// ==========================================
// INTERNAL STATE & MEMORY
// ==========================================
const int DELAY_SAMPLES = 4000;

bool take_snapshot = false;
float snapshot_raw[FRAME_SIZE];
float snapshot_processed[FRAME_SIZE];

int16_t rxFrame[FRAME_SIZE];      // being assembled by ESP-NOW callback
int16_t readyFrame[FRAME_SIZE];   // complete, waiting for loop
int16_t workFrame[FRAME_SIZE];    // loop's private copy
int16_t speexFrame[FRAME_SIZE];
float delay_buffer[DELAY_SAMPLES];

int delay_index = 0;
float env_level = 0.0f;
float gate_multiplier = 1.0f;
bool  gate_open = true;
float lpf_y_prev = 0.0f;
float od_pre1 = 0.0f, od_pre2 = 0.0f, od_post1 = 0.0f, od_post2 = 0.0f;
float od_dc_x = 0.0f, od_dc_y = 0.0f, od_drive_gain = 1.0f;
float med_z1 = 0.0f, med_z2 = 0.0f;
float dc_x_prev = 0.0f, dc_y_prev = 0.0f;
float hpf_x_prev = 0.0f, hpf_y_prev = 0.0f;

bool hardware_bypass = false;
unsigned long lastPacketTime = 0;
volatile bool frame_ready = false;
volatile int debug_packet_count = 0;
volatile bool speex_dirty = false;
SpeexPreprocessState *st;

int16_t last_good_p0[80], last_good_p1[80];
volatile uint32_t lost_packets = 0;
volatile bool packet_0_received = false;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

// ---- Pitch detector / synth state ----
float pitch_hist[PITCH_BUF];                       // rolling input history
float pitch_cs[PITCH_W + PITCH_MAXLAG + 1];        // prefix sum of squares
float pitch_nsdf[PITCH_MAXLAG + 2];                // normalised square difference

float synth_phase = 0.0f;          // 0..1
float synth_freq_cur = 110.0f;     // gliding frequency (Hz, before octave shift)
float synth_freq_target = 110.0f;  // latest detected / snapped frequency
float synth_env = 0.0f;            // input loudness follower
float synth_amp = 0.0f;            // smoothed synth amplitude
int   synth_hold = 0;              // frames of "voiced" left after last valid detection
int   synth_note = -1;             // current MIDI note (-1 = none)
float synth_f_hist[3] = {0, 0, 0}; // for median-of-3 octave-glitch rejection
float detected_freq_raw = 0.0f;    // last raw detection (for display)
float detected_clarity = 0.0f;

const char *NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// ==========================================
// ESP-NOW WIRELESS PROTOCOL
// ==========================================
typedef struct struct_message {
  uint8_t packet_id;
  int16_t audioSamples[80];
} struct_message;

static inline void publishFrame() {
  memcpy(readyFrame, rxFrame, sizeof(rxFrame));
  frame_ready = true;
}

void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(struct_message)) return;
  debug_packet_count++;
  const struct_message *p = (const struct_message *)data;

  portENTER_CRITICAL(&rxMux);
  if (p->packet_id == 0) {
    if (packet_0_received) {                 // previous packet 1 was lost
      memcpy(&rxFrame[80], last_good_p1, 80 * sizeof(int16_t));
      publishFrame();
      lost_packets++;
    }
    memcpy(&rxFrame[0], p->audioSamples, 80 * sizeof(int16_t));
    memcpy(last_good_p0, p->audioSamples, 80 * sizeof(int16_t));
    packet_0_received = true;
  } else if (p->packet_id == 1) {
    if (!packet_0_received) {                // packet 0 was lost
      memcpy(&rxFrame[0], last_good_p0, 80 * sizeof(int16_t));
      lost_packets++;
    }
    memcpy(&rxFrame[80], p->audioSamples, 80 * sizeof(int16_t));
    memcpy(last_good_p1, p->audioSamples, 80 * sizeof(int16_t));
    publishFrame();
    packet_0_received = false;
  }
  portEXIT_CRITICAL(&rxMux);
}

// ==========================================
// BLUETOOTH LISTENERS
// ==========================================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("Bluetooth Connected!");
    digitalWrite(LED_BLE_PIN, HIGH);
  };
  void onDisconnect(BLEServer *pServer) {
    Serial.println("Bluetooth Disconnected!");
    digitalWrite(LED_BLE_PIN, LOW);
    pServer->startAdvertising();
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() >= 3) {
      char tag = rxValue[0];
      float newValue = atof(rxValue.c_str() + 2);

      Serial.printf("BLE Update! Tag: [%c] | Value: %.4f\n", tag, newValue);

      switch (tag) {
        case 'G': MANUAL_GAIN_MULTIPLIER = newValue; break;
        case 'B': DC_BLOCKER_R = newValue; break;
        case 'H': HPF_ALPHA = newValue; break;
        case 'L': LPF_ALPHA = newValue; break;
        case 'T': GATE_THRESHOLD = newValue; break;
        case 'E': ENV_SMOOTH = newValue; break;
        case 'R': GATE_RELEASE_SPEED = newValue; break;
        case 'K': MEDIAN_ENABLED = (newValue > 0.5f); break;
        case 'A': GATE_ATTACK = newValue; break;
        case 'N':
          SPEEX_DENOISE_ON = (newValue > 0.5f) ? 1 : 0;
          speex_dirty = true;
          break;
        case 'S': {
          int db = -(int)fabsf(newValue);   // always negative
          if (db < -60) db = -60;
          SPEEX_NOISE_SUPPRESS_DB = db;
          speex_dirty = true;
          break;
        }
        case 'O': OVERDRIVE_ENABLED = (newValue > 0.5f); break;
        case 'D': OVERDRIVE_DRIVE = newValue; break;
        case 'V': OVERDRIVE_OUTPUT_GAIN = newValue; break;
        case 'Y': DELAY_ENABLED = (newValue > 0.5f); break;
        case 'M': DELAY_MIX = newValue; break;
        case 'F': DELAY_FEEDBACK = newValue; break;

        // ---- synthetic sound ----
        case 'Z': SYNTH_ENABLED = (newValue > 0.5f); break;
        case 'W': {
          int w = (int)newValue;
          if (w < 0) w = 0; if (w > 4) w = 4;
          SYNTH_WAVE = w;
          break;
        }
        case 'X': SYNTH_MIX = constrain(newValue, 0.0f, 1.0f); break;
        case 'I': SYNTH_LEVEL = newValue; break;
        case 'Q': {
          int o = (int)roundf(newValue);
          if (o < -2) o = -2; if (o > 2) o = 2;
          SYNTH_OCTAVE_MULT = powf(2.0f, (float)o);
          break;
        }
        case 'P': SYNTH_SNAP_TO_NOTE = (newValue > 0.5f); break;
        case 'U': SYNTH_GLIDE = constrain(newValue, 0.0001f, 1.0f); break;
        case 'C': SYNTH_CLARITY = constrain(newValue, 0.3f, 0.99f); break;
        case 'J': SYNTH_MIN_RMS = newValue; break;
      }
    }
  }
};

// ==========================================
// DSP MATH HELPERS
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
float soft_limit(float x) {                  // transparent below 18000
  const float knee = 18000.0f, ceil_ = 32000.0f;
  float a = fabsf(x);
  if (a <= knee) return x;
  float y = knee + (ceil_ - knee) * tanhf((a - knee) / (ceil_ - knee));
  return x < 0 ? -y : y;
}

// ==========================================
// PITCH DETECTION (McLeod NSDF)
// Returns true and fills freq/clarity when a clear pitch is found.
// ==========================================
bool detect_pitch(float *freq_out, float *clarity_out) {
  const int N = PITCH_W + PITCH_MAXLAG;
  const float *x = pitch_hist + (PITCH_BUF - N);   // most recent N samples

  // Prefix sum of squares (for fast normalisation term)
  pitch_cs[0] = 0.0f;
  for (int i = 0; i < N; i++) pitch_cs[i + 1] = pitch_cs[i] + x[i] * x[i];

  // Skip silence
  float rms = sqrtf((pitch_cs[N] - pitch_cs[N - PITCH_W]) / (float)PITCH_W);
  if (rms < SYNTH_MIN_RMS) return false;

  // NSDF for every lag
  for (int tau = 1; tau <= PITCH_MAXLAG; tau++) {
    float r = 0.0f;
    const float *a = x;
    const float *b = x + tau;
    for (int j = 0; j < PITCH_W; j++) r += a[j] * b[j];
    float m = (pitch_cs[PITCH_W] - pitch_cs[0]) + (pitch_cs[PITCH_W + tau] - pitch_cs[tau]);
    pitch_nsdf[tau] = (m > 1e-3f) ? (2.0f * r / m) : 0.0f;
  }
  pitch_nsdf[PITCH_MAXLAG + 1] = 0.0f;

  // Peak picking: skip first lobe (lag 0), then take max of each positive region
  int tau = 1;
  while (tau < PITCH_MAXLAG && pitch_nsdf[tau] > 0.0f) tau++;
  while (tau < PITCH_MAXLAG && pitch_nsdf[tau] <= 0.0f) tau++;

  int   pk_tau[24];
  float pk_val[24];
  int   np = 0;
  float max_val = 0.0f;

  while (tau < PITCH_MAXLAG) {
    if (pitch_nsdf[tau] > 0.0f) {
      int best = tau;
      while (tau < PITCH_MAXLAG && pitch_nsdf[tau] > 0.0f) {
        if (pitch_nsdf[tau] > pitch_nsdf[best]) best = tau;
        tau++;
      }
      if (best >= PITCH_MINLAG && best > 1 && np < 24) {
        pk_tau[np] = best;
        pk_val[np] = pitch_nsdf[best];
        if (pk_val[np] > max_val) max_val = pk_val[np];
        np++;
      }
    } else {
      tau++;
    }
  }

  if (np == 0 || max_val < SYNTH_CLARITY) return false;

  // First peak that is close to the highest one -> fundamental (avoids sub-octave errors)
  int chosen = -1;
  for (int i = 0; i < np; i++) {
    if (pk_val[i] >= 0.9f * max_val) { chosen = i; break; }
  }
  if (chosen < 0) return false;

  int t = pk_tau[chosen];
  float a = pitch_nsdf[t - 1], b = pitch_nsdf[t], c = pitch_nsdf[t + 1];
  float denom = a - 2.0f * b + c;
  float shift = (fabsf(denom) > 1e-9f) ? 0.5f * (a - c) / denom : 0.0f;
  float period = (float)t + shift;
  if (period < 1.0f) return false;

  *freq_out = (float)SAMPLE_RATE / period;
  *clarity_out = pk_val[chosen];
  return true;
}

// Feed one frame of (cleaned, un-gained) audio and update the synth's target pitch
void synth_update_pitch(const int16_t *frame) {
  // Slide history left by one frame, append the new frame
  memmove(pitch_hist, pitch_hist + FRAME_SIZE, (PITCH_BUF - FRAME_SIZE) * sizeof(float));
  for (int i = 0; i < FRAME_SIZE; i++) pitch_hist[PITCH_BUF - FRAME_SIZE + i] = (float)frame[i];

  bool was_voiced = (synth_hold > 0);
  if (synth_hold > 0) synth_hold--;

  float f, clarity;
  if (detect_pitch(&f, &clarity)) {
    detected_freq_raw = f;
    detected_clarity = clarity;

    // New note after silence: reset history so we lock on immediately
    if (!was_voiced) {
      synth_f_hist[0] = synth_f_hist[1] = synth_f_hist[2] = f;
      synth_note = -1;
    }
    // Median-of-3 rejects one-frame octave glitches
    synth_f_hist[0] = synth_f_hist[1];
    synth_f_hist[1] = synth_f_hist[2];
    synth_f_hist[2] = f;
    float fm = median_of_3(synth_f_hist[0], synth_f_hist[1], synth_f_hist[2]);

    // Nearest note with hysteresis (prevents flicker between two semitones)
    float midi = 69.0f + 12.0f * log2f(fm / 440.0f);
    if (synth_note < 0 || fabsf(midi - (float)synth_note) > 0.65f) {
      synth_note = (int)roundf(midi);
    }

    float out_f = fm;
    if (SYNTH_SNAP_TO_NOTE) {
      out_f = 440.0f * powf(2.0f, ((float)synth_note - 69.0f) / 12.0f);
    }
    synth_freq_target = out_f;
    if (!was_voiced) synth_freq_cur = out_f;   // no glide on onset
    synth_hold = 4;                            // stay voiced ~40 ms after last good detection
  }
}

// Render one sample of the synthetic voice. 'in' is the (gated, gained) guitar sample,
// used only as a loudness follower so the synth plays with the same dynamics.
float synth_process(float in) {
  float a = fabsf(in);
  synth_env += (a - synth_env) * ((a > synth_env) ? 0.05f : SYNTH_RELEASE);

  float amp_target = (synth_hold > 0) ? synth_env : 0.0f;
  synth_amp += (amp_target - synth_amp) * 0.02f;

  synth_freq_cur += (synth_freq_target - synth_freq_cur) * SYNTH_GLIDE;
  synth_phase += (synth_freq_cur * SYNTH_OCTAVE_MULT) / (float)SAMPLE_RATE;
  if (synth_phase >= 1.0f) synth_phase -= 1.0f;

  const float TWO_PI_F = 6.2831853f;
  float p = synth_phase, w;
  switch (SYNTH_WAVE) {
    case 0: w = sinf(TWO_PI_F * p); break;
    case 1: w = 4.0f * fabsf(p - 0.5f) - 1.0f; break;
    case 2: w = 2.0f * p - 1.0f; break;
    case 3: w = (p < 0.5f) ? 0.7f : -0.7f; break;
    default:
      w = 0.57f * (sinf(TWO_PI_F * p) + 0.5f * sinf(2.0f * TWO_PI_F * p) + 0.25f * sinf(3.0f * TWO_PI_F * p));
      break;
  }
  // 1.57 converts mean-absolute level to sine peak, so loudness roughly matches the guitar
  return w * synth_amp * 1.57f * SYNTH_LEVEL;
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(921600);

  pinMode(BYPASS_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_SWITCH_PIN, OUTPUT);
  pinMode(LED_BLE_PIN, OUTPUT);
  pinMode(LED_ESPNOW_PIN, OUTPUT);

  // I2S Audio
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
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

  // Speex Engine Init
  st = speex_preprocess_state_init(FRAME_SIZE, SAMPLE_RATE);
  speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &SPEEX_DENOISE_ON);
  speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &SPEEX_NOISE_SUPPRESS_DB);
  int off = 0;
  speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC, &off);
  speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_VAD, &off);

  // ESP-NOW Init
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  // BLE Init
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

  Serial.println("Airtone DSP Receiver Ready!");
}

// ==========================================
// MAIN AUDIO LOOP
// ==========================================
void loop() {

  // Snapshot timer (only if enabled)
  static unsigned long lastSnapshotTime = 0;
  if (SNAPSHOT_ENABLED && millis() - lastSnapshotTime > 4000) {
    take_snapshot = true;
    lastSnapshotTime = millis();
  }

  // 2-second status monitor
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 2000) {
    Serial.println("--- SYSTEM STATUS ---");
    Serial.printf("BLE Pin (14): %s\n", digitalRead(LED_BLE_PIN) ? "HIGH (ON)" : "LOW (OFF)");
    Serial.printf("ESP-NOW Packets (last 2s): %d\n", debug_packet_count);
    Serial.printf("Lost Packets: %lu\n", (unsigned long)lost_packets);
    Serial.printf("Speex: %s, %d dB\n", SPEEX_DENOISE_ON ? "ON" : "OFF", SPEEX_NOISE_SUPPRESS_DB);
    if (SYNTH_ENABLED) {
      if (synth_hold > 0 && synth_note >= 0) {
        int n = ((synth_note % 12) + 12) % 12;
        int oct = (synth_note / 12) - 1;
        Serial.printf("Synth: note %s%d | raw %.1f Hz | target %.1f Hz | clarity %.2f\n",
                      NOTE_NAMES[n], oct, detected_freq_raw, synth_freq_target, detected_clarity);
      } else {
        Serial.println("Synth: no note detected");
      }
    }
    debug_packet_count = 0;
    lastDebugTime = millis();
  }

  hardware_bypass = false;
  digitalWrite(LED_SWITCH_PIN, hardware_bypass ? HIGH : LOW);

  if (frame_ready) {
    portENTER_CRITICAL(&rxMux);
    memcpy(workFrame, readyFrame, sizeof(workFrame));
    frame_ready = false;
    portEXIT_CRITICAL(&rxMux);

    digitalWrite(LED_ESPNOW_PIN, HIGH);
    lastPacketTime = millis();

    // Apply pending Speex settings in the same task that runs Speex
    if (speex_dirty) {
      speex_dirty = false;
      speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &SPEEX_DENOISE_ON);
      speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &SPEEX_NOISE_SUPPRESS_DB);
    }

    unsigned long start_dsp_timer = take_snapshot ? micros() : 0;

    // ---- STAGE 1: cleanup (no gain, no gate) ----
    for (int i = 0; i < FRAME_SIZE; i++) {
      float x = (float)workFrame[i];
      if (MEDIAN_ENABLED) {
        float m = median_of_3(x, med_z1, med_z2);
        med_z2 = med_z1; med_z1 = x;
        x = m;
      }
      x = dc_block(x, &dc_x_prev, &dc_y_prev, DC_BLOCKER_R);
      x = high_pass(x, &hpf_x_prev, &hpf_y_prev, HPF_ALPHA);
      if (x > 32767.0f) x = 32767.0f; else if (x < -32768.0f) x = -32768.0f;
      speexFrame[i] = (int16_t)x;
    }

    // ---- SPEEX: sees the real noise floor so it can learn it ----
    if (SPEEX_DENOISE_ON) speex_preprocess_run(st, speexFrame);

    // ---- PITCH ANALYSIS: runs on the clean, un-gained signal ----
    if (SYNTH_ENABLED) synth_update_pitch(speexFrame);

    // ---- STAGE 2: tone, gain, gate, synth, effects ----
    for (int i = 0; i < FRAME_SIZE; i++) {
      float x = (float)speexFrame[i];

      // LPF
      x = (LPF_ALPHA * x) + ((1.0f - LPF_ALPHA) * lpf_y_prev);
      lpf_y_prev = x;

      // Gain
      float final_audio = x * MANUAL_GAIN_MULTIPLIER;

      // Smooth noise gate with hysteresis
      if (GATE_THRESHOLD > 0.0f) {
        env_level = ENV_SMOOTH * env_level + (1.0f - ENV_SMOOTH) * fabsf(final_audio);
        if (env_level > GATE_THRESHOLD) gate_open = true;
        else if (env_level < GATE_THRESHOLD * 0.7f) gate_open = false;
        if (gate_open) gate_multiplier += (1.0f - gate_multiplier) * GATE_ATTACK;
        else           gate_multiplier *= GATE_RELEASE_SPEED;
        final_audio *= gate_multiplier;
      }

      // Synthetic voice (follows detected note + guitar dynamics)
      if (SYNTH_ENABLED) {
        float s = synth_process(final_audio);
        final_audio = final_audio * (1.0f - SYNTH_MIX) + s * SYNTH_MIX;
      }

      // Tube overdrive
      if (OVERDRIVE_ENABLED && final_audio != 0.0f) {
        float od_x = final_audio / 32767.0f;
        float a_pre = 0.35f;
        od_pre1 = od_pre1 + a_pre * (od_x - od_pre1);
        od_pre2 = od_pre2 + a_pre * (od_pre1 - od_pre2);

        float driven = od_pre2 * OVERDRIVE_DRIVE;
        if (driven > 4.0f) driven = 4.0f; else if (driven < -4.0f) driven = -4.0f;

        float bias = 0.2f;
        float shaped = tanhf(driven + bias) - tanhf(bias);

        float dc_blocked = shaped - od_dc_x + 0.995f * od_dc_y;
        od_dc_x = shaped; od_dc_y = dc_blocked;

        float a_post = 0.25f;
        od_post1 = od_post1 + a_post * (dc_blocked - od_post1);
        od_post2 = od_post2 + a_post * (od_post1 - od_post2);

        float mixed = (od_pre2 * 0.3f) + (od_post2 * 0.7f);
        od_drive_gain = 1.0f / (1.0f + 0.15f * OVERDRIVE_DRIVE * OVERDRIVE_DRIVE);
        mixed *= od_drive_gain;

        if (mixed > 1.0f) mixed = 1.0f; else if (mixed < -1.0f) mixed = -1.0f;
        final_audio = mixed * OVERDRIVE_OUTPUT_GAIN * 32767.0f;
      }

      // Stadium delay
      if (DELAY_ENABLED) {
        float echo_audio = delay_buffer[delay_index];
        float stadium_sound = final_audio + (echo_audio * DELAY_MIX);
        delay_buffer[delay_index] = final_audio + (echo_audio * DELAY_FEEDBACK);
        delay_index++;
        if (delay_index >= DELAY_SAMPLES) delay_index = 0;
        final_audio = stadium_sound;
      }

      // Transparent limiter
      final_audio = soft_limit(final_audio);

      if (take_snapshot) {
        snapshot_raw[i] = (float)workFrame[i];
        snapshot_processed[i] = final_audio;
      }
      speexFrame[i] = (int16_t)final_audio;
    }

    size_t bytes_written;
    i2s_write(I2S_NUM_0, speexFrame, sizeof(speexFrame), &bytes_written, 10);

    // Print AFTER audio is written so it can't starve the DAC
    if (take_snapshot) {
      unsigned long dsp_duration = micros() - start_dsp_timer;
      take_snapshot = false;
      Serial.println("START_PLOT");
      Serial.print("TIME:"); Serial.println(dsp_duration);
      for (int j = 0; j < FRAME_SIZE; j++) {
        Serial.print(snapshot_raw[j]); Serial.print(",");
        Serial.println(snapshot_processed[j]);
      }
      Serial.println("END_PLOT");
    }
  }

  // ESP-NOW LED timeout
  if (millis() - lastPacketTime > 100) {
    digitalWrite(LED_ESPNOW_PIN, LOW);
  }
}