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

// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Deepgram URL with latest model, 5s VAD turnoff, linear16 encoding, 16kHz sample rate.
// Replace YOUR_API_KEY with your Deepgram API key.
const char* DEEPGRAM_URL = "wss://api.deepgram.com/v1/listen?model=latest&vad_turnoff=5000&encoding=linear16&sample_rate=16000&access_token=YOUR_API_KEY";

// This function streams I2S audio to Deepgram and returns the final transcript.
String streamDeepgramTranscript() {
  // Setup WebSocket client (disable cert verification)
  WebsocketsClient client;
  client.setInsecure();
  if (!client.connect(DEEPGRAM_URL)) {
    Serial.println("WebSocket Connection Failed");
    return "";
  }
  Serial.println("Connected to Deepgram");
  
  // I2S configuration
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
  
  i2s_driver_uninstall(I2S_PORT);
  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("I2S install error");
    return "";
  }
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
    Serial.println("I2S pin error");
    return "";
  }
  i2s_start(I2S_PORT);
  
  String finalTranscript = "";
  uint8_t buffer[BUF_SIZE];
  size_t bytesRead = 0;
  unsigned long lastSend = millis();
  
  while (true) {
    // Read audio from I2S and send as binary
    if (i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytesRead, 10) == ESP_OK && bytesRead > 0) {
      client.sendBinary(String((const char*)buffer, bytesRead));
      lastSend = millis();
    }
    
    // Check for incoming messages from Deepgram
    if (client.available()) {
      WebsocketsMessage msg = client.readBlocking();
      String incoming = msg.data();
      
      // Parse JSON response
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, incoming);
      if (!error) {
        bool isFinal = doc["channel"]["alternatives"][0]["is_final"];
        const char* transcript = doc["channel"]["alternatives"][0]["transcript"];
        if (transcript) {
          finalTranscript = transcript;
          Serial.printf("Transcript: %s\n", transcript);
        }
        if (isFinal) {
          Serial.println("Final transcript received.");
          break;
        }
      } else {
        Serial.printf("JSON error: %s\n", error.f_str());
      }
    }
    
    // Safeguard: break if no audio sent for 30 seconds
    if (millis() - lastSend > 30000) {
      Serial.println("Stream timeout.");
      break;
    }
  }
  
  i2s_driver_uninstall(I2S_PORT);
  client.close();
  return finalTranscript;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  
  String transcript = streamDeepgramTranscript();
  Serial.println("Final Transcript:");
  Serial.println(transcript);
}

void loop() {
  delay(1000);
}
