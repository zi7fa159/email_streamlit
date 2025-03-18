#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include <driver/i2s.h>

// WiFi Credentials
const char* ssid = "your-SSID";
const char* password = "your-PASSWORD";

// Deepgram API Key
const char* deepgramApiKey = "...";  // Replace with your actual API key

// I2S Configurations
#define I2S_PORT      I2S_NUM_0
#define I2S_WS        15
#define I2S_SCK       14
#define I2S_SD        32
#define SAMPLE_RATE   16000
#define SAMPLE_BITS   16
#define BYTES_PER_SAMPLE (SAMPLE_BITS / 8)
#define WAV_HDR_SIZE  44
#define RECORD_TIME   5
#define BUF_SIZE      512
#define FILENAME      "/rec.wav"

// Function to record and transcribe audio
String recordAndTranscribe() {
    Serial.println("Initializing SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed!");
        return "";
    }

    // Initialize I2S
    Serial.println("Initializing I2S...");
    i2s_driver_uninstall(I2S_PORT);
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = BUF_SIZE,
        .use_apll = false
    };
    i2s_pin_config_t pins = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };
    if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
        Serial.println("I2S installation failed!");
        return "";
    }
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
        Serial.println("I2S pin configuration failed!");
        return "";
    }
    i2s_start(I2S_PORT);

    // Record Audio
    Serial.println("Recording...");
    File file = SPIFFS.open(FILENAME, "w");
    if (!file) {
        Serial.println("Failed to create file.");
        return "";
    }
    uint8_t wavHeader[WAV_HDR_SIZE] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
        16, 0, 0, 0, 1, 0, 1, 0, (uint8_t)(SAMPLE_RATE & 0xFF), (uint8_t)((SAMPLE_RATE >> 8) & 0xFF),
        (uint8_t)((SAMPLE_RATE >> 16) & 0xFF), (uint8_t)((SAMPLE_RATE >> 24) & 0xFF),
        (uint8_t)((SAMPLE_RATE * BYTES_PER_SAMPLE) & 0xFF),
        (uint8_t)(((SAMPLE_RATE * BYTES_PER_SAMPLE) >> 8) & 0xFF),
        (uint8_t)(((SAMPLE_RATE * BYTES_PER_SAMPLE) >> 16) & 0xFF),
        (uint8_t)(((SAMPLE_RATE * BYTES_PER_SAMPLE) >> 24) & 0xFF),
        BYTES_PER_SAMPLE, 0, SAMPLE_BITS, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0 };
    file.write(wavHeader, WAV_HDR_SIZE);

    int16_t buffer[BUF_SIZE];
    size_t bytesRead = 0, totalBytes = 0;
    uint32_t startTime = millis();
    while (millis() - startTime < RECORD_TIME * 1000) {
        if (i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY) == ESP_OK && bytesRead > 0) {
            file.write((uint8_t*)buffer, bytesRead);
            totalBytes += bytesRead;
        }
    }
    file.close();
    Serial.printf("Recording complete: %u bytes written.\n", totalBytes);

    // Stop I2S
    i2s_driver_uninstall(I2S_PORT);

    // Transcribe Audio
    Serial.println("Connecting to Deepgram...");
    WiFiClientSecure *client = new WiFiClientSecure;
    client->setInsecure();
    HTTPClient https;
    if (!https.begin(*client, "https://api.deepgram.com/v1/listen?model=nova-2-general&detect_language=true")) {
        Serial.println("Failed to connect to Deepgram!");
        delete client;
        return "";
    }
    https.addHeader("Content-Type", "audio/wav");
    https.addHeader("Authorization", String("Token ") + deepgramApiKey);

    file = SPIFFS.open(FILENAME, "r");
    int httpCode = https.sendRequest("POST", &file, file.size());
    file.close();
    delete client;

    if (httpCode <= 0) {
        Serial.printf("HTTP request failed: %s\n", https.errorToString(httpCode).c_str());
        return "";
    }
    Serial.printf("HTTP response code: %d\n", httpCode);
    String response = https.getString();
    https.end();

    // Parse JSON response
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return "";
    }
    if (doc["results"]["channels"][0]["alternatives"][0].containsKey("transcript")) {
        String transcript = doc["results"]["channels"][0]["alternatives"][0]["transcript"].as<String>();
        Serial.println("Transcription: " + transcript);
        return transcript;
    } else {
        Serial.println("Transcription not found.");
        return "";
    }
}

// **Example Sketch to Test Function**
void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");

    Serial.println("\nStarting audio recording and transcription...");
    String result = recordAndTranscribe();
    if (result.length() > 0) {
        Serial.println("Final Transcript: " + result);
    } else {
        Serial.println("Transcription failed.");
    }
}

void loop() {
    delay(1000);
}
