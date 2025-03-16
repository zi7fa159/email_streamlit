#include <Arduino.h>
#include <driver/i2s.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
using namespace websockets;

#define I2S_PORT         I2S_NUM_0
#define I2S_WS           15
#define I2S_SCK          14
#define I2S_SD           32
#define SAMPLE_RATE      16000
#define SAMPLE_BITS      16
#define BYTES_PER_SAMPLE (SAMPLE_BITS/8)
#define BUF_SIZE         512

// Deepgram endpoint: latest model with VAD (5s turnoff)
const char* DEEPGRAM_ENDPOINT = "wss://api.deepgram.com/v1/listen?model=latest&vad_turnoff=5000&access_token=YOUR_API_KEY";

// Function: Streams I2S audio to Deepgram via websockets and returns the final transcript.
String streamDeepgramTranscript() {
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
  i2s_pin_config_t pin = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_uninstall(I2S_PORT);
  if(i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("I2S install error");
    return "";
  }
  if(i2s_set_pin(I2S_PORT, &pin) != ESP_OK) {
    Serial.println("I2S pin error");
    return "";
  }
  i2s_start(I2S_PORT);
  
  WebsocketsClient client;
  client.setReceiveTimeout(10000);
  if(!client.connect(DEEPGRAM_ENDPOINT)) {
    Serial.println("WS connect error");
    i2s_driver_uninstall(I2S_PORT);
    return "";
  }
  Serial.println("Connected to Deepgram");
  
  String finalTranscript = "";
  uint8_t buf[BUF_SIZE];
  size_t r = 0;
  unsigned long lastSend = millis();
  
  while(true) {
    if(i2s_read(I2S_PORT, buf, sizeof(buf), &r, 10) == ESP_OK && r > 0) {
      client.sendBinary((char*)buf, r);
      lastSend = millis();
    }
    String msg = client.poll();
    if(msg.length()) {
      DynamicJsonDocument doc(1024);
      DeserializationError err = deserializeJson(doc, msg);
      if(!err) {
        bool isFinal = doc["channel"]["alternatives"][0]["is_final"];
        const char* t = doc["channel"]["alternatives"][0]["transcript"];
        if(t) {
          finalTranscript = t;
          Serial.printf("Transcript: %s\n", t);
        }
        if(isFinal) {
          Serial.println("Final transcript received.");
          break;
        }
      } else {
        Serial.printf("JSON error: %s\n", err.f_str());
      }
    }
    if(millis() - lastSend > 30000) {
      Serial.println("Stream timeout.");
      break;
    }
  }
  
  i2s_driver_uninstall(I2S_PORT);
  client.disconnect();
  return finalTranscript;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 Deepgram Speech-to-Text Demo");
  
  String transcript = streamDeepgramTranscript();
  Serial.println("Final Transcript:");
  Serial.println(transcript);
}

void loop() {
  delay(1000);
}
