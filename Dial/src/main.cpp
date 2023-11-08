#include <Arduino.h>
#include <M5Dial.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>

M5GFX display;

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

struct espnow_tx
{
    byte id;
    int value;
} espnow_tx;

// Display Logic
bool render = true;
long oldPosition = 0;
int value = 0;
u8_t page = 0;

// Receive data into the struct and save it to the metrics array
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&espnow_rx, incomingData, len);
    render = true;
}

void drawPage()
{
    M5Dial.Display.clear();
    M5Dial.Display.setTextSize(1);
    switch (page)
    {
    case 0:
        M5Dial.Display.drawString("Speed", 120, 30);
        break;
    case 1:
        M5Dial.Display.drawString("AC Left", 120, 30);
        break;
    case 2:
        M5Dial.Display.drawString("AC Right", 120, 30);
        break;
    case 3:
        M5Dial.Display.drawString("CAN Queue", 120, 30);
        break;
    }
}

void setup()
{
    // put your setup code here, to run once:
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);
    M5Dial.Display.begin();

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

    M5Dial.Display.setRotation(1)
        M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setFont(&FreeSansBold12pt7b);
    drawPage();
}

void loop()
{
    M5Dial.update();
    long newPosition = M5Dial.Encoder.read() / 4;
    s8_t delta = newPosition - oldPosition;
    if (delta)
    {
        espnow_tx.id = page;
        switch (page)
        {
        case 0:
            break;
        case 1:
            espnow_tx.value = espnow_rx.hvac_left + (delta * 5);
            esp_now_send(senderAddress, (uint8_t *)&espnow_tx, sizeof(espnow_tx));
            break;
        case 2:
            espnow_tx.value = espnow_rx.hvac_right + (delta * 5);
            esp_now_send(senderAddress, (uint8_t *)&espnow_tx, sizeof(espnow_tx));
            break;
        case 3:
            break;
        }
    }
    if (M5Dial.BtnA.wasPressed())
    {
        page = (page + 1) % 4;
        render = true;
    }
    if (render)
    {
        render = false;
        M5Dial.Display.setTextSize(4);
        switch (page)
        {
        case 0:
            M5Dial.Display.drawString(String(espnow_rx.speed / 10) + "." + String(espnow_rx.speed % 10), 120, 120);
            break;
        case 1:
            M5Dial.Display.drawString(String(espnow_rx.hvac_left / 10) + "." + String(espnow_rx.hvac_left % 10), 120, 120);
            break;
        case 2:
            M5Dial.Display.drawString(String(espnow_rx.hvac_right / 10) + "." + String(espnow_rx.hvac_right % 10), 120, 120);
            break;
        case 3:
            M5Dial.Display.drawString(String(espnow_rx.queue / 10) + "." + String(espnow_rx.queue % 10), 120, 120);
            break;
        }
    }
}