#include <Arduino.h>
#include <M5Dial.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

Preferences preferences;

u8_t senderAddress[] = {0xD4, 0xD4, 0xDA, 0x9D, 0xFD, 0xE4};
esp_now_peer_info_t peerInfo;

bool render = true;
u32_t oldPosition = 0;
s16_t value = 0;
s16_t old_value = 0;
u32_t next_time = 0;
u8_t page = 0;

#define PAGES 8

// Receive data
void OnDataRecv(const u8_t *mac, const u8_t *data, int len)
{
    if (millis() < next_time)
        return; // Rate Limit
    next_time = millis() + 99;
    old_value = value;

    switch (page)
    {
    case 0: // Speed 12|12
        value = ((data[1] >> 4) + (data[2] << 4)) * 0.8 - 400;
        break;

    case 1: // AC Left 0|5
        value = (data[0] & B00011111) * 5 + 150;
        break;

    case 2: // AC Right 8|5
        value = (data[1] & B00011111) * 5 + 150;
        break;

    case 3:                                                    // Front Torque 21|13
        value = (data[1] + (data[2] & B00001111 << 8)) * 2.22; //+ (data[3] & B00000000 << 8)
        break;

    case 4:                                                    // Rear Torque 21|13
        value = (data[1] + (data[2] & B00001111 << 8)) * 2.22; // + (data[3] & B10000000 << 8)
        break;

    case 5: // HV Battery Voltage 0|16
        value = (data[0] | data[1] << 8) * 0.1;
        break;

    case 6: // HV Battery Current 16|16
        value = (data[2] | data[3] << 8);
        break;
    case 7: // Indicators
        value = data[0] >> 4;
        break;
    }
    render = value != old_value;

    // This is for debugging only
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("     " + String(data[0], HEX) + ":" + String(data[1], HEX) + ":" + String(data[2], HEX) + ":" + String(data[3], HEX) + "     ", 120, 180);
}

// Craft filters based on CAN Bus ID

const u16_t id_speed = 599, id_hvac = 755, id_fronttorque = 469, id_reartorque = 472, id_hvbattery = 306, id_lights = 1013;

void changePage()
{
    M5Dial.Display.setTextSize(4);
    M5Dial.Display.drawString("                                ", 120, 120);
    M5Dial.Display.setTextSize(1);
    switch (page)
    {
    case 0:
        esp_now_send(senderAddress, (u8_t *)&id_speed, 2);
        M5Dial.Display.drawString("     Speed     ", 120, 40);
        // M5Dial.Display.drawString("   KM/H   ", 120, 200);
        break;
    case 1:
        esp_now_send(senderAddress, (u8_t *)&id_hvac, 2);
        M5Dial.Display.drawString("   AC Left   ", 120, 40);
        // M5Dial.Display.drawString("  Celsius  ", 120, 200);
        break;
    case 2:
        esp_now_send(senderAddress, (u8_t *)&id_hvac, 2);
        M5Dial.Display.drawString("   AC Right   ", 120, 40);
        // M5Dial.Display.drawString("  Celsius  ", 120, 200);
        break;
    case 3:
        esp_now_send(senderAddress, (u8_t *)&id_fronttorque, 2);
        M5Dial.Display.drawString("   Front Torque   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    case 4:
        esp_now_send(senderAddress, (u8_t *)&id_reartorque, 2);
        M5Dial.Display.drawString("   Rear Torque   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    case 5:
        esp_now_send(senderAddress, (u8_t *)&id_hvbattery, 2);
        M5Dial.Display.drawString("   HV Voltage   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    case 6:
        esp_now_send(senderAddress, (u8_t *)&id_hvbattery, 2);
        M5Dial.Display.drawString("   HV Current   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    case 7:
        esp_now_send(senderAddress, (u8_t *)&id_lights, 2);
        M5Dial.Display.drawString("   Indicators   ", 120, 40);
        // M5Dial.Display.drawString("  Nm  ", 120, 200);
        break;
    }
    value = INT_MIN;
    next_time = 0;
    render = false;
}

void setup()
{
    // Setup WiFi for ESP Now
    WiFi.mode(WIFI_STA);

    // Setup persistant storage
    preferences.begin("dial", false);
    page = preferences.getUChar("page", 0);

    // Setup M5Dial
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);

    M5Dial.Display.setRotation(3);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setFont(&FreeSansBold12pt7b);

    // Start ESP Now
    if (esp_now_init() != ESP_OK)
    {
        M5Dial.Display.setColor(RED);
        M5Dial.Display.drawString("ESPNOW Failed", 120, 120);
        delay(1000);
        ESP.restart();
    }
    esp_now_register_recv_cb(OnDataRecv);

    // Setup ESP Now peer
    memcpy(peerInfo.peer_addr, senderAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        M5Dial.Display.setColor(RED);
        M5Dial.Display.drawString("Peer Failed", 120, 120);
        delay(1000);
    }

    // Draw the page
    changePage();
}

void loop()
{
    u16_t newPosition = M5Dial.Encoder.read() / 4;
    s8_t change = newPosition - oldPosition;
    if (change != 0)
    {
        page = (PAGES + page + change) % PAGES;
        preferences.putUChar("page", page);
        oldPosition = newPosition;
        changePage();
    }
    if (render)
    {
        switch (page)
        {
        case (7):
            M5Dial.Display.setTextSize(6);
            if (value & B1)
                M5Dial.Display.drawString("<", 120, 60);
            else if (value & B10)
                M5Dial.Display.drawString("<<", 120, 60);
            if (value & B100)
                M5Dial.Display.drawString(">", 120, 60);
            else if (value & B1000)
                M5Dial.Display.drawString(">>", 120, 60);
            break;
        default:
            M5Dial.Display.setTextSize(4);
            if (value < 2000) // Value less than 200.0
            {
                M5Dial.Display.drawString("  " + String(value / 10) + "." + String(abs(value) % 10) + "  ", 120, 120);
            }
            else // Value 200.0 or higher
            {
                M5Dial.Display.drawString("    " + String(value / 10) + "    ", 120, 120);
            }
            break;
        }
        render = false;
    }
}