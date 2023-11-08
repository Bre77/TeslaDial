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
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

// ESPNow data structures
struct espnow_tx
{
    int speed;
    int hvac_left;
    int hvac_right;
    int queue;
} espnow_tx;

struct espnow_rx
{
    byte id;
    int value;
} espnow_rx;

// CanBus Memory
CAN_device_t CAN_cfg;          // CAN Config
const int rx_queue_size = 256; // Receive Queue size
CAN_frame_t can_rx;            // CAN Frame for receiving
CAN_frame_t can_tx;            // CAN Frame for sending

// CanBus IDs
const u16_t id_speed = 599;
const u16_t id_hvac = 755;
const u16_t id_test = 0;

struct data_speed
{
    u16_t ignore1 : 12;
    u16_t speed : 12;
    u8_t ignore2 : 8;
} data_speed;

struct data_hvac
{
    u8_t hvac_left : 5;
    u8_t ignore1 : 3;
    u8_t hvac_right : 5;
    u8_t ignore2 : 3;
} data_hvac;

// LEDS
CRGB leds[NUM_LEDS];

// Timing
unsigned long next_send = 0;

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

bool send = false;
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    memcpy(&espnow_rx, data, 3);
    send = true;
    Serial.print("Bytes received: ");
    Serial.println(data_len);
    Serial.print("ID received: ");
    Serial.println(espnow_rx.id);
    Serial.print("Value received: ");
    Serial.println(espnow_rx.value);
}

void setup()
{
    auto cfg = M5.config();
    // cfg.serial_baudrate = 0;
    M5.begin(cfg);

    // Setup LED
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(20);
    led(CRGB::Blue);

    // Setup WiFi
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        error("Failed to start ESPNow");
    }
    Serial.println("Started ESPNow");

    esp_now_register_recv_cb(OnDataRecv);

    // Add broadcast peer
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        error("Failed to add broadcast peer");
    }
    Serial.println("Added broadcast peer");

    // CAN Bus
    CAN_cfg.speed = CAN_SPEED_500KBPS;
    CAN_cfg.tx_pin_id = GPIO_NUM_22;
    CAN_cfg.rx_pin_id = GPIO_NUM_19;
    CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));

    CAN_filter_t p_filter;
    p_filter.FM = Dual_Mode;

    ESP32Can.CANInit();
    ESP32Can.CANConfigFilter(&p_filter);
    delay(1000);

    u16_t id1 = id_speed;
    u16_t mask1 = (id_speed) ^ (id_speed);
    p_filter.ACR0 = id1 >> 3;
    p_filter.ACR1 = id1 << 5;
    p_filter.AMR0 = mask1 >> 3;
    p_filter.AMR1 = (mask1 << 5) + 15;

    u16_t id2 = id_hvac;
    u16_t mask2 = (id_hvac) ^ (id_hvac);
    p_filter.ACR2 = id2 >> 3;
    p_filter.ACR3 = id2 << 5;
    p_filter.AMR2 = mask2 >> 3;
    p_filter.AMR3 = (mask2 << 5) + 15;

    Serial.println("Filter 1");
    Serial.println(id1);
    Serial.println(mask1);
    Serial.println(p_filter.ACR0);
    Serial.println(p_filter.ACR1);
    Serial.println(p_filter.AMR0);
    Serial.println(p_filter.AMR1);
    Serial.println("");
    Serial.println("Filter 2");
    Serial.println(id2);
    Serial.println(mask2);
    Serial.println(p_filter.ACR2);
    Serial.println(p_filter.ACR3);
    Serial.println(p_filter.AMR2);
    Serial.println(p_filter.AMR3);
    Serial.println("");

    ESP32Can.CANConfigFilter(&p_filter);

    Serial.println("Started CANBus");

    led(CRGB::Green);
}

void loop()
{
    M5.update();

    if (xQueueReceive(CAN_cfg.rx_queue, &can_rx, 3 * portTICK_PERIOD_MS) == pdTRUE)
    {
        switch (can_rx.MsgID)
        {
        case id_speed:
        {
            // memcpy(&data_speed, can_rx.data.u8, 3);
            int speed1 = (can_rx.data.u8[1] & 0x0F) + (can_rx.data.u8[2] << 8);
            int speed2 = (can_rx.data.u8[1] & 0x0F) << 8 + can_rx.data.u8[2];
            Serial.println("RECIEVED");
            Serial.println(can_rx.data.u8[0]);
            Serial.println(can_rx.data.u8[1]);
            Serial.println(can_rx.data.u8[2]);
            Serial.println(speed1);
            Serial.println(speed1 * 0.8 - 400);
            Serial.println(speed2);
            Serial.println(speed2 * 0.8 - 400);

            espnow_tx.speed = (((can_rx.data.u8[1] & 0x0F) + (can_rx.data.u8[2] << 8)) * 0.8) - 400;
        }
        case id_hvac:
        {
            // memcpy(&data_hvac, can_rx.data.u8, 2);
            espnow_tx.hvac_left = ((can_rx.data.u8[0] >> 3) * 5) + 150;
            espnow_tx.hvac_right = ((can_rx.data.u8[1] >> 3) * 5) + 150;
            break;
        }
        default:
            // Serial.printf("WTF is: %d\n", can_rx.MsgID);
            break;
        }
    }
    else
    {
        // Serial.println("Nothing");
    }

    if (send)
    {
        Serial.println("Sending");
        Serial.println(espnow_rx.value);
        Serial.println((espnow_rx.value / 5) - 15);
        send = false;
        CAN_frame_t tx_frame;
        tx_frame.FIR.B.FF = CAN_frame_std;
        tx_frame.MsgID = id_hvac;
        tx_frame.FIR.B.DLC = 3;
        tx_frame.data.u8[0] = ((espnow_rx.value / 5) - 15) << 3;
        tx_frame.data.u8[1] = ((espnow_rx.value / 5) - 15) << 3;
        ESP32Can.CANWriteFrame(&tx_frame);
    }
    if (millis() > next_send)
    {
        next_send = millis() + 100;
        espnow_tx.queue = uxQueueMessagesWaiting(CAN_cfg.rx_queue);
        if (esp_now_send(broadcastAddress, (uint8_t *)&espnow_tx, sizeof(espnow_tx)) != ESP_OK)
        {
            Serial.println("Send Failed");
        }
    }

    if (M5.BtnA.isPressed())
    {
        led(CRGB::Purple);
        ESP32Can.CANStop();
        ESP.restart();
    }
}