#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <M5Unified.h>
#include <FastLED.h>
#include <ESP32CAN.h>
#include <CAN_config.h>

#define NUM_LEDS 1
#define DATA_PIN 27

// D4:D4:DA:9D:FD:E4
const uint8_t d1_address[6] = {0x48, 0x27, 0xE2, 0xE3, 0xC9, 0x20};
const uint8_t d2_address[6] = {0x48, 0x27, 0xE2, 0xE3, 0xAA, 0xEC};
esp_now_peer_info_t peerInfo;
const byte d1_check = 0x20;
const byte d2_check = 0xEC;

u16_t d1_id, d2_id; // Requested Message IDs

// CanBus Memory
CAN_device_t CAN_cfg;         // CAN Config
CAN_frame_t can_rx;           // CAN Frame for receiving
const int rx_queue_size = 16; // Receive Queue size
CAN_filter_t p_filter;        // Filters

// LEDS
CRGB leds[NUM_LEDS];

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
    Serial.print(mac_addr[0]);
    Serial.print(mac_addr[1]);
    Serial.print(mac_addr[2]);
    Serial.print(mac_addr[3]);
    Serial.print(mac_addr[4]);
    Serial.println(mac_addr[5]);
    switch (mac_addr[5])
    {
    case d1_check:

        d1_id = data[0] << 8 + data[1];
        p_filter.ACR0 = d1_id >> 3;
        p_filter.ACR1 = d1_id << 5;
        ESP32Can.CANConfigFilter(&p_filter);
        led(CRGB::Yellow);
        break;

    case d2_check:

        d2_id = data[0] << 8 + data[1];
        p_filter.ACR0 = d2_id >> 3;
        p_filter.ACR1 = d2_id << 5;
        ESP32Can.CANConfigFilter(&p_filter);
        led(CRGB::Purple);
        break;

    default:
        led(CRGB::Red);
        break;
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
    Serial.println(WiFi.macAddress());
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
        Serial.print("ID: ");
        Serial.print(can_rx.MsgID);
        Serial.print(" D1: ");
        Serial.print(d1_id);
        Serial.print(" D2: ");
        Serial.println(d2_id);

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