#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <M5Unified.h>
#include <FastLED.h>
#include <ESP32CAN.h>
#include <CAN_config.h>

#define NUM_LEDS 1
#define DATA_PIN 27

// 50:02:91:92:94:D8
const uint8_t d1_address[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t d2_address[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;
const byte d1_check = 0xAA;
const byte d2_check = 0xBB;

// ESPNow data structures
/*struct espnow_tx
{
    u16_t id;     // Message ID
    u8_t data[8]; // Message Data
} d1_tx, d2_tx;*/

u16_t d1_id, d2_id; // Requested Message IDs

// CanBus Memory
CAN_device_t CAN_cfg;          // CAN Config
CAN_frame_t can_rx;            // CAN Frame for receiving
const int rx_queue_size = 256; // Receive Queue size
CAN_filter_t p_filter;

// LEDS
CRGB leds[NUM_LEDS];

// Timing
unsigned long next_send = 0;
bool send = false;

// Helper to set Button LED
void led(CRGB color)
{
    leds[0] = color;
    FastLED.show();
}

void error(String msg = "Error")
{
    Serial.println(msg);
    ESP32Can.CANStop();
    for (int i = 0; i < 50; i++)
    {
        led(CRGB::Red);
        delay(100);
        led(CRGB::Black);
        delay(100);
    }
    Serial.println("Restarting");
    ESP.restart();
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    switch (mac_addr[5])
    {
    case d1_check:
    {
        d1_id = data[0];
        p_filter.ACR0 = d1_id >> 3;
        p_filter.ACR1 = d1_id << 5;
        ESP32Can.CANConfigFilter(&p_filter);
        break;
    }
    case d2_check:
    {
        d2_id = data[0];
        p_filter.ACR0 = d2_id >> 3;
        p_filter.ACR1 = d2_id << 5;
        ESP32Can.CANConfigFilter(&p_filter);
        break;
    }
    }
}

void setup()
{
    // Setup M5 Atom
    auto cfg = M5.config();
    M5.begin(cfg);

    // Setup Button LED
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(20);
    led(CRGB::Blue);

    // Setup WiFi
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
        error("Failed to start ESPNow");

    // ESPNow callback
    esp_now_register_recv_cb(OnDataRecv);

    // ESP Now Peers
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    memcpy(peerInfo.peer_addr, d1_address, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
        error("Failed to add peer");
    memcpy(peerInfo.peer_addr, d2_address, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
        error("Failed to add peer");

    // CAN Bus Setup
    CAN_cfg.speed = CAN_SPEED_500KBPS;
    CAN_cfg.tx_pin_id = GPIO_NUM_22;
    CAN_cfg.rx_pin_id = GPIO_NUM_19;
    CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
    p_filter.FM = Dual_Mode;
    p_filter.AMR0 = 0;
    p_filter.AMR1 = B1111; // Ignore Data bits
    p_filter.AMR2 = 0;
    p_filter.AMR3 = B1111; // Ignore Data bits
    ESP32Can.CANConfigFilter(&p_filter);
    ESP32Can.CANInit();

    // Signal Ready
    led(CRGB::Green);
}

void loop()
{
    if (xQueueReceive(CAN_cfg.rx_queue, &can_rx, 3 * portTICK_PERIOD_MS) == pdTRUE)
    {
        if (can_rx.MsgID == d1_id)
        {
            esp_now_send(d1_address, can_rx.data.u8, can_rx.FIR.B.DLC);
        }
        if (can_rx.MsgID == d2_id)
        {
            esp_now_send(d2_address, can_rx.data.u8, can_rx.FIR.B.DLC);
        }
    }
}