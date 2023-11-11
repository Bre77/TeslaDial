#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <M5Unified.h>
#include <FastLED.h>
#include <ESP32CAN.h>
#include <CAN_config.h>

#define NUM_LEDS 1
#define DATA_PIN 27

esp_now_peer_info_t peerInfo;

const uint8_t d1_address[6] = {0x48, 0x27, 0xE2, 0xE3, 0xC9, 0x20};
const uint8_t d2_address[6] = {0x48, 0x27, 0xE2, 0xE3, 0xAA, 0xEC};
const byte d1_check = 0x20; // Last byte of MAC Address
const byte d2_check = 0xEC; // Last byte of MAC Address

u16_t d1_id, d2_id;                    // Requested Message IDs
u8_t d1_data[8], d2_data[8];           // Data to send
u8_t d1_length, d2_length;             // Data length
bool d1_ready = true, d2_ready = true; // Ready to send
bool restart_can = false;              // Restart CAN Bus

// CanBus Memory
CAN_device_t CAN_cfg;         // CAN Config
CAN_frame_t can_rx;           // CAN Frame for receiving
CAN_filter_t p_filter;        // Filters
const int rx_queue_size = 16; // Receive Queue size

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
    switch (mac_addr[5])
    {
    case d1_check:
        d1_id = data[0] + (data[1] << 8);
        p_filter.ACR0 = d1_id >> 3;
        p_filter.ACR1 = d1_id << 5;
        restart_can = true;
        Serial.println(d1_id);
        break;
    case d2_check:
        d2_id = data[0] + (data[1] << 8);
        p_filter.ACR2 = d2_id >> 3;
        p_filter.ACR3 = d2_id << 5;
        restart_can = true;
        Serial.println(d2_id);
        break;
    }
}

void OnDataSend(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS)
        led(CRGB::Red);
    switch (mac_addr[5])
    {
    case d1_check:
        d1_ready = true;
        break;
    case d2_check:
        d2_ready = true;
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
    esp_now_register_send_cb(OnDataSend);

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

#define RATELIMIT 199
long d1_time, d2_time;

void loop()
{
    if (restart_can)
    {
        Serial.println("Restarting CAN");
        restart_can = false;
        led(CRGB::Yellow);
        ESP32Can.CANStop();
        ESP32Can.CANConfigFilter(&p_filter);
        ESP32Can.CANInit();
        led(CRGB::Green);
    }
    if (xQueueReceive(CAN_cfg.rx_queue, &can_rx, 3 * portTICK_PERIOD_MS) == pdTRUE)
    {
        Serial.print(can_rx.MsgID);
        Serial.print(" ");
        Serial.print(d1_ready);
        Serial.print(" ");
        Serial.println(d2_ready);
        if (d1_ready & can_rx.MsgID == d1_id) // millis() > d1_time
        {
            d1_ready = false;
            d1_time = millis() + RATELIMIT;
            esp_now_send(d1_address, (u8_t *)&can_rx.data.u8, can_rx.FIR.B.DLC);
        }
        if (d2_ready & can_rx.MsgID == d2_id) // millis() > d2_time
        {
            d2_ready = false;
            d2_time = millis() + RATELIMIT;
            esp_now_send(d2_address, (u8_t *)&can_rx.data.u8, can_rx.FIR.B.DLC);
        }
    }
}