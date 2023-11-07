#include <Arduino.h>
#include <M5Dial.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>

uint8_t senderAddress[] = {0x50, 0x02, 0x91, 0x92, 0x94, 0xD8};
esp_now_peer_info_t peerInfo;

// ESPNow data structures
struct espnow_rx
{
    int speed;
    int hvac_left;
    int hvac_right;
    int queue;
} espnow_rx;

// Receive data into the struct and save it to the metrics array
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&espnow_rx, incomingData, len);
    Serial.println(espnow_rx.speed);
    Serial.println(espnow_rx.hvac_left);
    Serial.println(espnow_rx.hvac_right);
    Serial.println(espnow_rx.queue);
    Serial.println("");
}

void setup()
{
    // put your setup code here, to run once:
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Failed to start ESPNow");
        return;
    }
    Serial.println("Started ESPNow");

    // esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    memcpy(peerInfo.peer_addr, senderAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add sender peer");
        return;
    }
    Serial.println("Added sender peer");

    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextSize(4);
}
long oldPosition = 0;
int value = 0;

void loop()
{

    M5Dial.update();

    M5Dial.Display.drawString(String(espnow_rx.hvac_left), (M5Dial.Display.width() / 2) - 60, 120);
    M5Dial.Display.drawString(String(espnow_rx.hvac_right), (M5Dial.Display.width() / 2) + 60, 120);

    long newPosition = M5Dial.Encoder.read() / 4;
    if (newPosition != oldPosition)
    {
        value += (newPosition - oldPosition);
        M5Dial.Display.clear();
        M5Dial.Display.drawString(String(value), M5Dial.Display.width() / 2, 30);
        oldPosition = newPosition;
    }
    if (M5Dial.BtnA.wasPressed())
    {
        M5Dial.Encoder.readAndReset();
    }
    if (M5Dial.BtnA.pressedFor(5000))
    {
        M5Dial.Encoder.write(100);
    }
}