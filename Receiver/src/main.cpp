/**
 * AirTone Receiver — Fixed Gate + Clean Amplification
 * 
 * Removed the broken compressor.
 * Uses a proper gate that STAYS CLOSED on noise.
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <driver/i2s.h>

#define I2S_BCLK_PIN   26
#define I2S_LRC_PIN    25
#define I2S_DOUT_PIN   22
#define BUFFER_SIZE    64
#define SAMPLE_RATE_HZ 16000

// ─── TUNING PARAMETERS ───
// 
// Based on your data, your noise floor peaks at ~50-60.
// So gate threshold MUST be well above that to keep noise silent.
// 
#define GATE_THRESHOLD   50     // Well above noise floor of ~60
#define GATE_RELEASE     0.90f   // Faster release (was 0.995 — too slow)
#define LPF_ALPHA        0.40f
#define DC_FILTER_R      0.998f
#define OUTPUT_GAIN      15      // Fixed gain, no compressor

typedef struct {
    int16_t samples[BUFFER_SIZE];
} AudioPacket;

AudioPacket rxPacket;

float lpf_out       = 0.0f;
float prev_input    = 0.0f;
float dc_out        = 0.0f;
float gate_envelope = 0.0f;

void setupI2S() {
    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE_HZ,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 4,
        .dma_buf_len          = BUFFER_SIZE,
        .use_apll             = false,
        .tx_desc_auto_clear   = true
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = I2S_BCLK_PIN,
        .ws_io_num    = I2S_LRC_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    memcpy(&rxPacket, data, sizeof(AudioPacket));

    int16_t outBuffer[BUFFER_SIZE];
    int32_t chunkPeak = 0;
    float   cleanSignal[BUFFER_SIZE];

    // Pass 1: filter and find peak
    for (int i = 0; i < BUFFER_SIZE; i++) {
        float input = (float)rxPacket.samples[i];

        // Low-pass (kills hiss)
        lpf_out = (LPF_ALPHA * input) + ((1.0f - LPF_ALPHA) * lpf_out);

        // DC removal (kills offset drift)
        dc_out = lpf_out - prev_input + (DC_FILTER_R * dc_out);
        prev_input = lpf_out;

        cleanSignal[i] = dc_out;

        int32_t absVal = (int32_t)fabsf(dc_out);
        if (absVal > chunkPeak) chunkPeak = absVal;
    }

    // Gate decision — STRICT threshold
    // Only opens when we DEFINITELY have a signal, not noise
    if (chunkPeak > GATE_THRESHOLD) {
        gate_envelope = 1.0f;
    } else {
        gate_envelope *= GATE_RELEASE;
    }

    // Pass 2: gate + amplify
    for (int i = 0; i < BUFFER_SIZE; i++) {
        float gated = cleanSignal[i] * gate_envelope;
        int32_t boosted = (int32_t)(gated * OUTPUT_GAIN);

        if      (boosted >  32767) boosted =  32767;
        else if (boosted < -32768) boosted = -32768;

        outBuffer[i] = (int16_t)boosted;
    }

    size_t bytes_written;
    i2s_write(I2S_NUM_0, outBuffer, sizeof(outBuffer), &bytes_written, 0);

    // Debug — show gate status clearly
    static int counter = 0;
    if (counter++ % 30 == 0) {
        Serial.print("Peak=");
        Serial.print(chunkPeak);
        Serial.print("  Gate=");
        Serial.print(gate_envelope > 0.1 ? "OPEN " : "SHUT ");
        Serial.print("  Env=");
        Serial.print(gate_envelope, 2);
        Serial.print("  Out=");
        Serial.println(outBuffer[0]);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("[RX] Fixed gate version");
    Serial.printf("Gate=%d  Gain=%d\n", GATE_THRESHOLD, OUTPUT_GAIN);
    setupI2S();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_now_init();
    esp_now_register_recv_cb(onDataReceived);
}

void loop() { delay(1); }