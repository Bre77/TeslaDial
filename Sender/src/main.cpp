#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <M5Unified.h>
#include <FastLED.h>
#include <ESP32CAN.h>
#include <CAN_config.h>

#define NUM_LEDS 1
#define DATA_PIN 27

CAN_device_t CAN_cfg;           // CAN Config
const int rx_queue_size = 2048; // Receive Queue size
int rx_queue_count = 0;

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

// CanBus Message
CAN_frame_t rx_frame;

// CanBus IDs
const u16_t id_speed = 599;
const u16_t id_hvac = 755;

struct
{
    int ignore : 12;
    int speed : 12;
} data_speed;

struct
{
    u8_t hvac_left : 5;
    u8_t hvac_right : 5;
} data_hvac;

// LEDS
CRGB leds[NUM_LEDS];

void led(CRGB color)
{
    leds[0] = color;
    FastLED.show();
}

void error(String msg = "Error")
{
    Serial.println(msg);
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
    CAN.cfg.
    CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));

    CAN_filter_t p_filter;
    p_filter.FM = Single_Mode;

    p_filter.ACR0 = (id_speed) & 255;
    p_filter.ACR1 = (id_speed) >> 8;
    p_filter.ACR2 = 0;
    p_filter.ACR3 = 0;

    p_filter.AMR0 = (id_speed) & 255;
    p_filter.AMR1 = (id_speed) >> 8;
    p_filter.AMR2 = 0;
    p_filter.AMR3 = 0;

    //ESP32Can.CANConfigFilter(&p_filter);
    ESP32Can.CANInit();

    Serial.println("Started CANBus");

    led(CRGB::Black);
}

void loop()
{
    M5.update();

    if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE)
    {
        switch (rx_frame.MsgID)
        {
        case id_speed:
            Serial.printf("Speed: %d\n", rx_frame.data.u8[0]);
            break;
        case id_hvac:
            Serial.printf("HVAC Left: %d Right: %d\n", rx_frame.data.u8[0], rx_frame.data.u8[1]);
            break;
        default:
            //Serial.printf("WTF is: %d\n", rx_frame.MsgID);
            break;
        }
        /*Serial.printf(" from 0x%08X, DLC %d, Data ", rx_frame.MsgID, rx_frame.FIR.B.DLC);
            for (int i = 0; i < rx_frame.FIR.B.DLC; i++)
            {
                Serial.printf("0x%02X ", rx_frame.data.u8[i]);
            }
            Serial.printf("\n");*/

        /*if (rx_frame.data.u8[0] == 0x01) {
            CAN_frame_t tx_frame;
            tx_frame.FIR.B.FF   = CAN_frame_std;
            tx_frame.MsgID      = CAN_MSG_ID;
            tx_frame.FIR.B.DLC  = 8;
            tx_frame.data.u8[0] = 0x02;
            tx_frame.data.u8[1] = rx_frame.data.u8[1];
            tx_frame.data.u8[2] = 0x00;
            tx_frame.data.u8[3] = 0x00;
            tx_frame.data.u8[4] = 0x00;
            tx_frame.data.u8[5] = 0x00;
            tx_frame.data.u8[6] = 0x00;
            tx_frame.data.u8[7] = 0x00;
            ESP32Can.CANWriteFrame(&tx_frame);
        }*/
    }
    else
    {
        Serial.println("Nothing");
    }

    rx_queue_count = uxQueueSpacesAvailable(CAN_cfg.rx_queue);
    led(CRGB(0, map(rx_queue_count, 0, rx_queue_size, 0, 255), 0));

    /*if (esp_now_send(broadcastAddress, (uint8_t *)&data_send, sizeof(data_send)) == ESP_OK)
    {
        leds[0] = CRGB::Blue;
        FastLED.show();
    }
    else
    {
        Serial.println("Failed to send");
    }*/
    delay(50);
}