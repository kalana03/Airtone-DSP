#include <Arduino.h>
#include <driver/i2s.h>
#include <speex/speex_preprocess.h> 

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

// --- 1. Median Filter Variables ---
float med_z1 = 0.0f;
float med_z2 = 0.0f;

// --- 2. Float Filter Variables ---
float dc_x_prev = 0.0f, dc_y_prev = 0.0f;
const float DC_BLOCKER_R = 0.995f; 

float hpf_x_prev = 0.0f, hpf_y_prev = 0.0f;
const float HPF_ALPHA = 0.962f; 

// --- 3. Speex Variables ---
SpeexPreprocessState *st;

// --- DSP Math Functions ---

// Fast 3-sample median sorter
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

    int denoise = 1;
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &denoise);

    int noise_suppress = -25; 
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);

    int agc = 1; 
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC, &agc);

    float agc_level = 2000; 
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC_LEVEL, &agc_level);

    int vad = 1; 
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
    .dma_buf_count = 4, // Kept low for minimum latency
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

void setup() {
  Serial.begin(115200);
  MySerial.setRxBufferSize(2048); 
  MySerial.begin(460800, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  
  setupI2S();
  audio_denoise_init(); 
  Serial.println("🎧 DSP Pipeline: Median -> DC Block -> HPF -> Speex");
}

void loop() {
  
  // The Elastic Flush: Dump old memory to snap back to real-time
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
          
          // Step 1: Median Filter (Kill single-sample pops BEFORE they ring)
          float med_clean = median_of_3(raw, med_z1, med_z2);
          
          // Shift history for the next sample
          med_z2 = med_z1;
          med_z1 = raw; 
          
          // Step 2: DC Block
          float dc_clean = dc_block(med_clean, &dc_x_prev, &dc_y_prev, DC_BLOCKER_R);
          
          // Step 3: High-Pass Filter
          float hpf_clean = high_pass(dc_clean, &hpf_x_prev, &hpf_y_prev, HPF_ALPHA);
          
          speexFrame[i] = (int16_t)hpf_clean; 
        }
        
        // Step 4: Speex Denoise & AGC
        speex_preprocess_run(st, speexFrame); 
        
        // Step 5: Output to Speaker
        size_t bytes_written;
        i2s_write(I2S_NUM_0, speexFrame, sizeof(speexFrame), &bytes_written, 10);

        Serial.print(">WiredAudio:");
        Serial.println(speexFrame[0]); 
      }
    }
  }
}