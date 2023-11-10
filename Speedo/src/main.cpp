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
int oldvalue = 0;
long lasttime = 0;
u8_t page = 0;

#define PAGES 7

// Receive data
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len)
{
    if (millis() < lasttime)
        return;
    lasttime = millis() + 99;
    oldvalue = value;
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("-----" + String(data[0], HEX) + ":" + String(data[1], HEX) + ":" + String(data[2], HEX) + ":" + String(data[3], HEX) + "-----", 120, 180);
    switch (page)
    {
    case (0): // Speed 12|12
        value = ((data[1] >> 4) + (data[2] << 4)) * 0.8 - 400;
        break;

    case (1): // AC Left 0|5
        value = (data[0] & B00011111) * 5 + 150;
        break;

    case (2): // AC Right 8|5
        value = (data[1] & B00011111) * 5 + 150;
        break;

    case (3): // Front Torque 21|13
        value = ((data[2] & B11111000 >> 3) + (data[3] << 8)) * 2.22;
        break;

    case (4): // Rear Torque 21|13
        value = ((data[2] & B11111000 >> 3) + (data[3] << 8)) * 2.22;
        break;

    case (5): // HV Battery Voltage 0|16
        value = ((data[0]) + (data[1] << 8)) * 0.1;
        break;

    case (6): // HV Battery Current 16|16
        value = ((data[2]) + (data[3] << 8)) * -1;
        break;
    }
    render = value != oldvalue;
}

// Craft filters based on CAN Bus ID

const u16_t id_speed = 599, id_hvac = 755, id_fronttorque = 469, id_reartorque = 472, id_hvbattery = 306;

void changePage()
{
    M5Dial.Display.setTextSize(4);
    M5Dial.Display.drawString("                                ", 120, 120);
    M5Dial.Display.setTextSize(1);
    switch (page)
    {
    case 0:
        esp_now_send(senderAddress, (uint8_t *)&id_speed, 2);
        M5Dial.Display.drawString("     Speed     ", 120, 40);
        // M5Dial.Display.drawString("   KM/H   ", 120, 200);
        break;
    case 1:
        esp_now_send(senderAddress, (uint8_t *)&id_hvac, 2);
        M5Dial.Display.drawString("   AC Left   ", 120, 40);
        // M5Dial.Display.drawString("  Celsius  ", 120, 200);
        break;
    case 2:
        esp_now_send(senderAddress, (uint8_t *)&id_hvac, 2);
        M5Dial.Display.drawString("   AC Right   ", 120, 40);
        // M5Dial.Display.drawString("  Celsius  ", 120, 200);
        break;
    case 3:
        esp_now_send(senderAddress, (uint8_t *)&id_fronttorque, 2);
        M5Dial.Display.drawString("   Front Torque   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    case 4:
        esp_now_send(senderAddress, (uint8_t *)&id_reartorque, 2);
        M5Dial.Display.drawString("   Rear Torque   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    case 5:
        esp_now_send(senderAddress, (uint8_t *)&id_hvbattery, 2);
        M5Dial.Display.drawString("   HV Voltage   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    case 6:
        esp_now_send(senderAddress, (uint8_t *)&id_hvbattery, 2);
        M5Dial.Display.drawString("   HV Current   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    }
    value = INT_MIN;
    lasttime = 0;
    render = false;
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