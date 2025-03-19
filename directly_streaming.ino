#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include <driver/i2s.h>

// WiFi credentials – replace with your own
const char* ssid = "your-SSID";
const char* password = "your-PASSWORD";

// Deepgram API key – replace with your actual key
const char* deepgramApiKey = "...";

// I2S and recording settings
#define I2S_PORT      I2S_NUM_0
#define I2S_WS        15
#define I2S_SCK       14
#define I2S_SD        32
#define SAMPLE_RATE   16000
#define SAMPLE_BITS   16
#define BYTES_PER_SAMPLE (SAMPLE_BITS / 8)
#define WAV_HDR_SIZE  44
#define RECORD_TIME   5      // seconds
#define BUF_SIZE      512
#define FILENAME      "/rec.wav"

// Single function that records audio, updates the WAV header,
// sends the file to Deepgram, and returns the transcript.
String recordAndTranscribe() {
  // Initialize SPIFFS for file storage
  Serial.println("Initializing SPIFFS...");
  if (!SPIFFS.begin(true)) {
    Serial.println("ERROR - SPIFFS initialization failed!");
    return "";
  }
  
  // Uninstall I2S driver if installed (ignore warning if not)
  i2s_driver_uninstall(I2S_PORT);
  
  // Initialize I2S for recording
  Serial.println("Initializing I2S...");
  i2s_config_t i2s_config = {
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
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("ERROR - I2S installation failed!");
    return "";
  }
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
    Serial.println("ERROR - I2S pin configuration failed!");
    return "";
  }
  i2s_start(I2S_PORT);

  // Open file for recording; first write a temporary WAV header.
  Serial.println("Recording...");
  File audioFile = SPIFFS.open(FILENAME, "w");
  if (!audioFile) {
    Serial.println("ERROR - Failed to open file for writing");
    return "";
  }
  // Write a temporary header (will be updated later)
  uint8_t wavHeader[WAV_HDR_SIZE] = {0};
  audioFile.write(wavHeader, WAV_HDR_SIZE);

  // Record audio data for RECORD_TIME seconds
  int16_t buffer[BUF_SIZE];
  size_t bytesRead = 0, totalAudioBytes = 0;
  uint32_t startTime = millis();
  while (millis() - startTime < RECORD_TIME * 1000) {
    if (i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY) == ESP_OK && bytesRead > 0) {
      audioFile.write((uint8_t*)buffer, bytesRead);
      totalAudioBytes += bytesRead;
    }
  }
  
  // Update WAV header with actual data size and file size information
  uint32_t fileSize = totalAudioBytes + WAV_HDR_SIZE - 8;
  wavHeader[0] = 'R'; wavHeader[1] = 'I'; wavHeader[2] = 'F'; wavHeader[3] = 'F';
  wavHeader[4] = fileSize & 0xFF; 
  wavHeader[5] = (fileSize >> 8) & 0xFF; 
  wavHeader[6] = (fileSize >> 16) & 0xFF; 
  wavHeader[7] = (fileSize >> 24) & 0xFF;
  wavHeader[8] = 'W'; wavHeader[9] = 'A'; wavHeader[10] = 'V'; wavHeader[11] = 'E';
  wavHeader[12] = 'f'; wavHeader[13] = 'm'; wavHeader[14] = 't'; wavHeader[15] = ' ';
  wavHeader[16] = 16; wavHeader[17] = 0; wavHeader[18] = 0; wavHeader[19] = 0;
  wavHeader[20] = 1; wavHeader[21] = 0; 
  wavHeader[22] = 1; wavHeader[23] = 0;
  wavHeader[24] = SAMPLE_RATE & 0xFF; 
  wavHeader[25] = (SAMPLE_RATE >> 8) & 0xFF; 
  wavHeader[26] = (SAMPLE_RATE >> 16) & 0xFF; 
  wavHeader[27] = (SAMPLE_RATE >> 24) & 0xFF;
  uint32_t byteRate = SAMPLE_RATE * BYTES_PER_SAMPLE;
  wavHeader[28] = byteRate & 0xFF; 
  wavHeader[29] = (byteRate >> 8) & 0xFF; 
  wavHeader[30] = (byteRate >> 16) & 0xFF; 
  wavHeader[31] = (byteRate >> 24) & 0xFF;
  wavHeader[32] = BYTES_PER_SAMPLE; 
  wavHeader[33] = 0;
  wavHeader[34] = SAMPLE_BITS; 
  wavHeader[35] = 0;
  wavHeader[36] = 'd'; wavHeader[37] = 'a'; wavHeader[38] = 't'; wavHeader[39] = 'a';
  wavHeader[40] = totalAudioBytes & 0xFF; 
  wavHeader[41] = (totalAudioBytes >> 8) & 0xFF; 
  wavHeader[42] = (totalAudioBytes >> 16) & 0xFF; 
  wavHeader[43] = (totalAudioBytes >> 24) & 0xFF;
  audioFile.seek(0);
  audioFile.write(wavHeader, WAV_HDR_SIZE);
  audioFile.close();
  Serial.printf("Recording complete: %u bytes written.\n", totalAudioBytes);

  // Stop I2S now that recording is done
  i2s_driver_uninstall(I2S_PORT);

  // Connect to Deepgram to transcribe the audio
  Serial.println("Connecting to Deepgram...");
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure(); // For demo purposes only; use proper certificate validation in production
  HTTPClient https;
  if (!https.begin(*client, "https://api.deepgram.com/v1/listen?model=nova-2-general&detect_language=true")) {
    Serial.println("ERROR - HTTPS setup failed");
    delete client;
    return "";
  }
  https.addHeader("Content-Type", "audio/wav");
  https.addHeader("Authorization", String("Token ") + deepgramApiKey);

  // Open the recorded file for reading and send it
  audioFile = SPIFFS.open(FILENAME, "r");
  int httpCode = https.sendRequest("POST", &audioFile, audioFile.size());
  audioFile.close();
  Serial.printf("HTTP response code: %d\n", httpCode);
  String response = https.getString();
  https.end();
  delete client;

  // Check if the response is empty before parsing to avoid crashing
  if(response.length() == 0) {
    Serial.println("ERROR - Empty response from Deepgram.");
    return "";
  }
  
  // Parse the JSON response from Deepgram
  DynamicJsonDocument doc(4096);
  DeserializationError jsonError = deserializeJson(doc, response);
  if (jsonError) {
    Serial.print("JSON parsing failed: ");
    Serial.println(jsonError.c_str());
    return "";
  }
  
  // Extract and return the transcript if available
  if (doc["results"]["channels"][0]["alternatives"][0].containsKey("transcript")) {
    String transcript = doc["results"]["channels"][0]["alternatives"][0]["transcript"].as<String>();
    Serial.println("Transcription: " + transcript);
    return transcript;
  } else {
    Serial.println("ERROR - Transcript not found in the JSON response.");
    return "";
  }
}

// Example sketch to test the bundled function.
// NOTE: Ensure this function is called only once (e.g. from setup)
// to avoid repeated reinitializations that might cause instability.
void setup() {
  Serial.begin(115200);
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  // Call the record and transcribe function
  Serial.println("\nStarting audio recording and transcription...");
  String result = recordAndTranscribe();
  if (result.length() > 0) {
    Serial.println("Final Transcript: " + result);
  } else {
    Serial.println("Transcription failed.");
  }
}

void loop() {
  // Do nothing here to avoid calling the function repeatedly.
  delay(1000);
}
