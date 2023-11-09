#include <Arduino.h>
#include <M5Dial.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>

// ESP CAN is xD4:D4:DA:9D:FD:E4
uint8_t senderAddress[] = {0xD4, 0xD4, 0xDA, 0x9D, 0xFD, 0xE4};
esp_now_peer_info_t peerInfo;

bool render = true;
long oldPosition = 0;
int value = 0;
u8_t page = 0;

#define PAGES 4

// Receive data
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len)
{
    M5Dial.Display.drawNumber(data[0], 30, 120);
    switch (page)
    {
    case (0): // Speed
        value = ((data[1] >> 4) | (data[2] << 4)) * 0.8 - 400;
        break;

    case (1): // AC Left
        value = (data[0] >> 3) * 5 + 150;
        break;

    case (2): // AC Right
        value = (data[1] >> 3) * 5 + 150;
        break;
    }
    render = true;
}

// Craft filters based on CAN Bus ID
const u16_t id_blank = 0;
const u16_t id_speed = 599;
const u16_t id_hvac = 755;

void changePage()
{
    M5Dial.Display.setTextSize(1);
    switch (page)
    {
    case 0:
        esp_now_send(senderAddress, (uint8_t *)&id_speed, 2);
        M5Dial.Display.drawString("  Speed  ", 120, 30);
        M5Dial.Display.drawString("   KM/H   ", 120, 200);
        break;
    case 1:
        esp_now_send(senderAddress, (uint8_t *)&id_hvac, 2);
        M5Dial.Display.drawString("  AC Left  ", 120, 30);
        M5Dial.Display.drawString("  Celsius  ", 120, 200);
        break;
    case 2:
        esp_now_send(senderAddress, (uint8_t *)&id_hvac, 2);
        M5Dial.Display.drawString("  AC Right  ", 120, 30);
        M5Dial.Display.drawString("  Celsius  ", 120, 200);
        break;
    case 3:
        esp_now_send(senderAddress, (uint8_t *)&id_blank, 2);
        M5Dial.Display.drawString("   Blank   ", 120, 30);
        M5Dial.Display.drawString("  Blank  ", 120, 200);
        break;
    }

    render = true;
}

void setup()
{
    // put your setup code here, to run once:
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);
    M5Dial.Display.setRotation(3);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setFont(&FreeSansBold12pt7b);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        M5Dial.Display.setColor(RED);
        M5Dial.Display.drawString("WiFi Failed", 120, 120);
        delay(1000);
    }

    esp_now_register_recv_cb(OnDataRecv);

    memcpy(peerInfo.peer_addr, senderAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        M5Dial.Display.setColor(RED);
        M5Dial.Display.drawString("Peer Failed", 120, 120);
        delay(1000);
    }

    changePage();
}

void loop()
{
    long newPosition = M5Dial.Encoder.read() / 4;
    s8_t change = newPosition - oldPosition;
    if (change != 0)
    {
        page = (PAGES + page + change) % PAGES;
        oldPosition = newPosition;
        changePage();
    }
    if (render)
    {
        M5Dial.Display.setTextSize(4);
        M5Dial.Display.drawString("  " + String(value / 10) + "." + String(value % 10) + "  ", 120, 120);
        render = false;
    }
}