#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <M5AtomDisplay.h>
#include <M5Unified.h>
#include <FastLED.h>

#define NUM_LEDS 1
#define DATA_PIN 27

// 50:02:91:92:94:D8

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

typedef struct esp_now_msg
{
    byte id;
    int value;
} esp_now_msg;

// Create a struct_message called myData
esp_now_msg data_send;
esp_now_msg data_receive;

CRGB leds[NUM_LEDS];

void error()
{
    for (int i = 0; i < 50; i++)
    {
        leds[0] = CRGB::Red;
        delay(100);
        leds[0] = CRGB::Black;
        delay(100);
    }
    ESP.restart();
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    memcpy(&data_receive, data, 4);
    Serial.print("Bytes received: ");
    Serial.println(data_len);
    Serial.print("Data received: ");
    Serial.println(data_receive.value);
    Serial.print("Data received: ");
    Serial.println(data_receive.id);
}

void setup()
{
    auto cfg = M5.config();
    // cfg.serial_baudrate = 0;
    M5.begin(cfg);

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(20);
    leds[0] = CRGB::Green;
    FastLED.show();

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESPNow Init Failed");
        error();
    }
    Serial.println("ESPNow Init Success");

    esp_now_register_recv_cb(OnDataRecv);

    // Add broadcast peer
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        error();
    }
    Serial.println("Added peer");
    Serial.println(sizeof(&data_send));
}

void loop()
{
    M5.update();
    data_send.id = random(0, 25);
    data_send.value = random(0, 32767);
    if (esp_now_send(broadcastAddress, (uint8_t *)&data_send, sizeof(data_send)) == ESP_OK)
    {
        leds[0] = CRGB::Blue;
        FastLED.show();
    }
    else
    {
        Serial.println("Failed to send");
    }
    delay(50);
}