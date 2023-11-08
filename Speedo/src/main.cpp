#include <Arduino.h>
#include <M5Dial.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>

#define PAGES 4

uint8_t senderAddress[] = {0x50, 0x02, 0x91, 0x92, 0x94, 0xD8};
esp_now_peer_info_t peerInfo;

bool render = true;
long oldPosition = 0;
int value = 0;
u8_t page = 0;
// const char titles[PAGES] = {"Speed", "AC Left", "AC Right", "CAN Queue" };

// Receive data into the struct and save it to the metrics array
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len)
{
    switch (page)
    {
        case(0){
            value = data[3];
            break;
        }
        case(1){
            value = data[0] >> 3;
            break;
        }
        case(2){
            value = data[1] >> 3;
            break;
        }
    }
}



void drawPage()
{
    M5Dial.Display.clear();
    M5Dial.Display.setTextSize(1);
    switch (page)
    {
    case 0:
            M5Dial.Display.drawString("Speed", 120, 30);
            M5Dial.Display.drawString("KM/H", 120, 200);
            break;
        case 1:
            M5Dial.Display.drawString("AC Left", 120, 30);
            M5Dial.Display.drawString("Celsius", 120, 200);
            break;
        case 2:
            M5Dial.Display.drawString("AC Right", 120, 30);
            M5Dial.Display.drawString("Celsius", 120, 200);
            break;
        case 3:
            M5Dial.Display.drawString("CAN Queue", 120, 30);
            M5Dial.Display.drawString("Frames", 120, 200);
            break;
        }

        render = true;
}

void setup()
{
    // put your setup code here, to run once:
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
        ESP.restart();

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

    M5Dial.Display.setRotation(3);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setFont(&FreeSansBold12pt7b);
    drawPage();
}

void loop()
{
    M5Dial.update();
    long newPosition = M5Dial.Encoder.read() / 4;
    s8_t change = newPosition - oldPosition;
    if (change != 0)
    {
        page = (PAGES + page + change) % PAGES;
        Serial.println(page);
        oldPosition = newPosition;
        drawPage();
    }
    if (render)
    {
        render = false;
        M5Dial.Display.setTextSize(4);
        switch (page)
        {
        case 0:
            M5Dial.Display.drawString("  " + String(espnow_rx.speed / 10) + "." + String(espnow_rx.speed % 10) + "  ", 120, 120);
            break;
        case 1:
            M5Dial.Display.drawString("  " + String(espnow_rx.hvac_left / 10) + "." + String(espnow_rx.hvac_left % 10) + "  ", 120, 120);
            break;
        case 2:
            M5Dial.Display.drawString("  " + String(espnow_rx.hvac_right / 10) + "." + String(espnow_rx.hvac_right % 10) + "  ", 120, 120);
            break;
        case 3:
            M5Dial.Display.drawString("  " + String(espnow_rx.queue) + "  ", 120, 120);
            break;
        }
    }
}