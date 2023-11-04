#include <Arduino.h>
#include <esp_now.h>
#include <M5Dial.h>
#include <WiFi.h>

#define METRICS 25

uint8_t senderAddress[] = {0x50, 0x02, 0x91, 0x92, 0x94, 0xD8};
esp_now_peer_info_t peerInfo;

typedef struct esp_now_msg
{
    byte id;
    int value;
} esp_now_msg;

esp_now_msg receive;
esp_now_msg send;

int metrics[METRICS];

void error()
{
    for (int i = 0; i < 50; i++)
    {
        M5.dis.fillpix(0xff0000);
        delay(100);
        M5.dis.fillpix(0x000000);
        delay(100);
    }
    ESP.restart();
}

// Receive data into the struct and save it to the metrics array
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&data, incomingData, 3);
    metrics[data.id] = data.value;
}

void setup()
{
    // put your setup code here, to run once:
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESPNow Init Failed");
        return;
    }
    Serial.println("ESPNow Init Success");

    // esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    memcpy(peerInfo.peer_addr, senderAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }
    Serial.println("Added peer");
    Serial.println(sizeof(data))
}

void loop()
{
    M5.update()
}