#include <Arduino.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

#define I2S_PORT         I2S_NUM_0
#define I2S_WS           15
#define I2S_SCK          14
#define I2S_SD           32
#define SAMPLE_RATE      16000
#define SAMPLE_BITS      16
#define BUF_SIZE         512

// WiFi and Deepgram credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* DEEPGRAM_KEY = "YOUR_API_KEY";
const char* DEEPGRAM_URL = "wss://api.deepgram.com/v1/listen?model=latest&vad_turnoff=5000&encoding=linear16&sample_rate=16000";

String streamDeepgramTranscript() {
  // Initialize WebSocket client with authorization header
  WebsocketsClient client;
  client.setInsecure();
  client.addHeader("Authorization", "Token " + String(DEEPGRAM_KEY));
  client.addHeader("Content-Type", "audio/raw");
  
  // Connect with retry mechanism
  for (int i = 0; i < 3; i++) {
    Serial.printf("Connection attempt %d...\n", i + 1);
    if (client.connect(DEEPGRAM_URL)) {
      Serial.println("Connected to Deepgram.");
      break;
    }
    if (i == 2) {
      Serial.println("Connection failed after 3 attempts.");
      return "";
    }
    delay(1000 * (i + 1));
  }
  
  // Configure I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUF_SIZE,
    .use_apll = false
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  
  // Setup I2S
  i2s_driver_uninstall(I2S_PORT);
  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK ||
      i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
    Serial.println("I2S setup failed");
    client.close();
    return "";
  }
  i2s_start(I2S_PORT);
  
  // Main processing variables
  String finalTranscript = "";
  uint8_t buffer[BUF_SIZE];
  size_t bytesRead = 0;
  unsigned long lastSend = millis();
  unsigned long startTime = millis();
  
  // Main loop
  while (millis() - startTime < 60000) { // 60 second max runtime
    // Poll for WebSocket events
    client.poll();
    
    // Check connection status
    if (!client.available()) {
      Serial.println("Connection lost");
      break;
    }
    
    // Read audio from I2S
    if (i2s_read(I2S_PORT, buffer, BUF_SIZE, &bytesRead, 10) == ESP_OK && bytesRead > 0) {
      if (client.sendBinary((const char*)buffer, bytesRead)) {
        lastSend = millis();
      }
    }
    
    // Process incoming messages
    if (client.available()) {
      WebsocketsMessage msg = client.readBlocking();
      if (msg.isText()) {
        // Parse JSON response
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, msg.data());
        
        if (!error && doc.containsKey("channel")) {
          JsonArray alternatives = doc["channel"]["alternatives"];
          if (alternatives.size() > 0) {
            bool isFinal = alternatives[0]["is_final"];
            const char* transcript = alternatives[0]["transcript"];
            
            if (transcript && strlen(transcript) > 0) {
              finalTranscript = transcript;
              Serial.println("Transcript: " + finalTranscript);
            }
            
            if (isFinal) {
              Serial.println("Final transcript received");
              break;
            }
          }
        }
      }
    }
    
    // Timeout if no audio sent for 5 seconds
    if (millis() - lastSend > 5000) {
      Serial.println("Audio timeout");
      break;
    }
  }
  
  // Cleanup
  i2s_stop(I2S_PORT);
  i2s_driver_uninstall(I2S_PORT);
  client.close();
  
  return finalTranscript;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 Deepgram Speech-to-Text Example ===");
  
  // Connect to WiFi with timeout
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
    
    // Call the Deepgram streaming function
    Serial.println("\nStarting audio transcription...");
    String transcript = streamDeepgramTranscript();
    
    // Display results
    Serial.println("\n=== Transcription Results ===");
    if (transcript.length() > 0) {
      Serial.println("Transcript: " + transcript);
    } else {
      Serial.println("No transcript received or error occurred");
    }
  } else {
    Serial.println("\nWiFi connection failed");
  }
}

void loop() {
  // Nothing to do here, everything happens in setup
  delay(1000);
}
