#include <Arduino.h>
#include <M5Dial.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

Preferences preferences;

u8_t senderAddress[] = {0xD4, 0xD4, 0xDA, 0x9D, 0xFD, 0xE4};
esp_now_peer_info_t peerInfo;

bool render = false;
int32_t oldPosition = 0;
int32_t value = INT32_MAX;
int32_t old_value = INT32_MAX;
unsigned long next_time = 0;
uint8_t page = 0;
bool dark = true;
bool decimal = true;

void ExtractValue(u8_t start, u8_t length, u8_t *data)
{
    value = 0;
    u8_t offset = start % 8;
    for (u8_t bit = start; bit < (start + length); bit++)
    {
        value += (data[bit / 8] >> (bit % 8) & 1) << (bit - start);
    }
}

#define PAGES 13

#define PAGE_SPEED 0
#define PAGE_HV_BATTERY_POWER 1
#define PAGE_HV_BATTERY_VOLTAGE 2
#define PAGE_HV_BATTERY_CURRENT 3
#define PAGE_REAR_TORQUE 4
#define PAGE_REAR_POWER 5
#define PAGE_REAR_AMPS 6
#define PAGE_FRONT_TORQUE 7
#define PAGE_FRONT_POWER 8
#define PAGE_FRONT_AMPS 9
#define PAGE_CABIN_TEMP 10
#define PAGE_ACTUAL_TEMP 11
#define PAGE_OUTSIDE_TEMP 12

#define PAGE_INDICATORS 13
#define PAGE_AC_LEFT 101
#define PAGE_AC_RIGHT 102

// Receive data
void OnDataRecv(const u8_t *mac, const u8_t *data, int len)
{
    if (millis() < next_time)
        return; // Rate Limit
    next_time = millis() + 99;
    old_value = value;

    /* 0|16
    |   LSB  |  MSB   |
    |76543210|76543210|
    */
    /* 4|8
     |  LSB   |  MSB   |
     |7654____|____3210|
     */

    // Signed numbers are two's complement

    switch (page)
    {
    case PAGE_SPEED: // 12|12@1+
        value = (data[1] >> 4 | data[2] << 4) * 0.8 - 400;
        decimal = true;
        break;
    case PAGE_AC_LEFT: // 0|5@1+
        value = (data[0] & 31) * 5 + 150;
        decimal = true;
        break;
    case PAGE_AC_RIGHT: // 8|5@1+
        value = (data[1] & 31) * 5 + 150;
        decimal = true;
        break;
    case PAGE_FRONT_POWER: // 0|11@1- (0.5,0) [-512|511.5]
        value = (s16_t)(data[0] << 5 | data[1] << 13) / 32 * 5;
        decimal = true;
        break;
    case PAGE_FRONT_TORQUE: // 21|13@1- (0.222,0) [-909.312|909.09] "NM"  Receiver
        // value = (data[1] + (data[2] & B00001111 << 8)) * 2.22; //+ (data[3] & B00000000 << 8)
        // value = (data[2] >> 5 | data[3] << 3 | data[4] & 1 << 11) * (data[4] & 2) ? 2.22 : -2.22;
        value = (s16_t)(data[2] >> 2 | data[3] << 6 | data[4] << 14) * 0.2775;
        decimal = true;
        break;
    case PAGE_REAR_POWER: // 0|11@1- (0.5,0) [-512|511.5]
        value = (s16_t)(data[0] << 5 | data[1] << 13) / 32 * 5;
        decimal = true;
        break;
    case PAGE_REAR_TORQUE: // 21|13@1- (0.222,0) [-909.312|909.09] "NM"  Receiver
        // value = (data[1] + (data[2] & B00001111 << 8)) * 2.22; // + (data[3] & B10000000 << 8)
        // value = (data[2] >> 5 | data[3] << 3 | data[4] & 1 << 11) * (data[4] & 2) ? 2.22 : -2.22;
        value = (s16_t)(data[2] >> 2 | data[3] << 6 | data[4] << 14) * 0.2775;
        decimal = true;
        break;
    case PAGE_FRONT_AMPS: // 11|11@1+ (1,0) [0|2047]
        value = (data[1] >> 5 | data[2] << 3) * 10;
        decimal = false;
        break;
    case PAGE_REAR_AMPS: // 11|11@1+ (1,0) [0|2047]
        value = (data[1] >> 5 | data[2] << 3) * 10;
        decimal = false;
        break;
    case PAGE_HV_BATTERY_POWER: // Voltage * Current / 1000
        value = -round((data[0] | data[1] << 8) * (int16_t)(data[2] | data[3] << 8) / 10000) / 10;
        decimal = true;
        break;
    case PAGE_HV_BATTERY_VOLTAGE: // 0|16@1+ (0.01,0) [0|655.35]
        value = (data[0] | data[1] << 8) * 0.1;
        decimal = true;
        break;
    case PAGE_HV_BATTERY_CURRENT: // 16|16@1- (-0.1,0) [-3276.7|3276.7]
        value = -(int16_t)(data[2] | data[3] << 8);
        decimal = true;
        break;
    case PAGE_INDICATORS: // 4|4?
        value = data[0] & 15;
        break;
    case PAGE_CABIN_TEMP: // 30|11@1+ (0.1,-40) NEED TO IMPLEMENT MULTIPLEXING
        value = (data[3] >> 6 + data[4] << 2 + (data[5] & 1) << 10) - 400;
        decimal = true;
        break;
    case PAGE_ACTUAL_TEMP: // 1|8@1- (1,-40)
        value = (s8_t)(data[0] >> 1 | data[1] << 7) * 10 - 400;
        decimal = false;
        break;
    case PAGE_OUTSIDE_TEMP: // 24|8@1+ (0.5,-40)
        value = data[3] * 5 - 400;
        decimal = true;
        break;
    }
    render = value != old_value;

    // This is for debugging only
    // M5Dial.Display.setTextSize(0.5);
    // M5Dial.Display.drawString("     " + String(data[0], HEX) + ":" + String(data[1], HEX) + ":" + String(data[2], HEX) + ":" + String(data[3], HEX) + "     ", 120, 200);
}

// Craft filters based on CAN Bus ID

const u16_t id_time = 1320;
const u16_t id_speed = 599;
const u16_t id_hvac_request = 755;
const u16_t id_front_power = 741;
const u16_t id_front_torque = 469;
const u16_t id_rear_power = 614;
const u16_t id_rear_torque = 472;
const u16_t id_hv_battery = 306;
const u16_t id_lights = 1013;
const u16_t id_hvac_status[2] = {579, 3 << 8};
const u16_t id_ths = 899;
const u16_t id_front_sensor = 801;
const u16_t id_front_amps = 421;
const u16_t id_rear_amps = 294;

void drawPage()
{
    M5Dial.Display.clear();
    // M5Dial.Display.drawString("100.0", 120, 125, &FreeSansBold24pt7b);
    M5Dial.Display.setTextSize(1);
    switch (page)
    {
    case PAGE_SPEED:
        esp_now_send(senderAddress, (u8_t *)&id_speed, 2);
        M5Dial.Display.drawString("Speed", 120, 50);
        M5Dial.Display.drawString("KM/H", 120, 180);
        break;
    case PAGE_AC_LEFT:
        esp_now_send(senderAddress, (u8_t *)&id_hvac_request, 2);
        M5Dial.Display.drawString("AC Left", 120, 50);
        M5Dial.Display.drawString("Celsius", 120, 180);
        break;
    case PAGE_AC_RIGHT:
        esp_now_send(senderAddress, (u8_t *)&id_hvac_request, 2);
        M5Dial.Display.drawString("AC Right", 120, 50);
        M5Dial.Display.drawString("Celsius", 120, 180);
        break;
    case PAGE_FRONT_POWER:
        esp_now_send(senderAddress, (u8_t *)&id_front_power, 2);
        M5Dial.Display.drawString("Front", 120, 20);
        M5Dial.Display.drawString("Power", 120, 50);
        M5Dial.Display.drawString("kW", 120, 180);
        break;
    case PAGE_REAR_POWER:
        esp_now_send(senderAddress, (u8_t *)&id_rear_power, 2);
        M5Dial.Display.drawString("Rear", 120, 20);
        M5Dial.Display.drawString("Power", 120, 50);
        M5Dial.Display.drawString("kW", 120, 180);
        break;
    case PAGE_FRONT_TORQUE:
        esp_now_send(senderAddress, (u8_t *)&id_front_torque, 2);
        M5Dial.Display.drawString("Front", 120, 20);
        M5Dial.Display.drawString("Torque", 120, 50);
        M5Dial.Display.drawString("Nm", 120, 180);
        break;
    case PAGE_REAR_TORQUE:
        esp_now_send(senderAddress, (u8_t *)&id_rear_torque, 2);
        M5Dial.Display.drawString("Rear", 120, 20);
        M5Dial.Display.drawString("Torque", 120, 50);
        M5Dial.Display.drawString("Nm", 120, 180);
        break;
    case PAGE_FRONT_AMPS:
        esp_now_send(senderAddress, (u8_t *)&id_front_amps, 2);
        M5Dial.Display.drawString("Front", 120, 20);
        M5Dial.Display.drawString("Motor", 120, 50);
        M5Dial.Display.drawString("Amps", 120, 180);
        break;
    case PAGE_REAR_AMPS:
        esp_now_send(senderAddress, (u8_t *)&id_rear_amps, 2);
        M5Dial.Display.drawString("Rear", 120, 20);
        M5Dial.Display.drawString("Motor", 120, 50);
        M5Dial.Display.drawString("Amps", 120, 180);
        break;
    case PAGE_HV_BATTERY_POWER:
        esp_now_send(senderAddress, (u8_t *)&id_hv_battery, 2);
        M5Dial.Display.drawString("HV", 120, 20);
        M5Dial.Display.drawString("Power", 120, 50);
        M5Dial.Display.drawString("kW", 120, 180);
        break;
    case PAGE_HV_BATTERY_VOLTAGE:
        esp_now_send(senderAddress, (u8_t *)&id_hv_battery, 2);
        M5Dial.Display.drawString("HV", 120, 20);
        M5Dial.Display.drawString("Voltage", 120, 50);
        M5Dial.Display.drawString("Volts", 120, 180);
        break;
    case PAGE_HV_BATTERY_CURRENT:
        esp_now_send(senderAddress, (u8_t *)&id_hv_battery, 2);
        M5Dial.Display.drawString("HV", 120, 20);
        M5Dial.Display.drawString("Current", 120, 50);
        M5Dial.Display.drawString("Amps", 120, 180);
        break;
    case PAGE_INDICATORS:
        esp_now_send(senderAddress, (u8_t *)&id_lights, 2);
        M5Dial.Display.drawString("Indicators", 120, 50);
        break;
    case PAGE_CABIN_TEMP:
        esp_now_send(senderAddress, (u8_t *)&id_hvac_status, 4);
        M5Dial.Display.drawString("Cabin", 120, 50);
        M5Dial.Display.drawString("Temp", 120, 20);
        M5Dial.Display.drawString("Celsius", 120, 180);
        break;
    case PAGE_ACTUAL_TEMP:
        esp_now_send(senderAddress, (u8_t *)&id_ths, 2);
        M5Dial.Display.drawString("Inside", 120, 50);
        M5Dial.Display.drawString("Temp", 120, 20);
        M5Dial.Display.drawString("Celsius", 120, 180);
        break;
    case PAGE_OUTSIDE_TEMP:
        esp_now_send(senderAddress, (u8_t *)&id_front_sensor, 2);

        M5Dial.Display.drawString("Outside", 120, 50);
        M5Dial.Display.drawString("Temp", 120, 20);
        M5Dial.Display.drawString("Celsius", 120, 180);
        break;
    }
    value = INT16_MAX;
    next_time = 0;
    render = false;
}

void setup()
{
    // Setup WiFi for ESP Now
    WiFi.mode(WIFI_STA);

    // Setup persistant storage
    preferences.begin("dial", false);
    page = preferences.getUChar("page", 0) % PAGES;
    dark = preferences.getBool("dark", true);

    // Setup M5Dial
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);

    M5Dial.Display.setRotation(3);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setFont(&Orbitron_Light_24); // FreeSansBold12pt7b
    M5Dial.Display.invertDisplay(dark);

    // Start ESP Now
    if (esp_now_init() != ESP_OK)
    {
        M5Dial.Display.clear(RED);
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
        M5Dial.Display.clear(RED);
        M5Dial.Display.drawString("Peer Failed", 120, 120);
        delay(1000);
        ESP.restart();
    }

    u16_t count = 0;
    while (!render)
    {
        M5Dial.Display.drawString("Connecting", 120, 100);
        M5Dial.Display.drawString("  " + String(count) + "  ", 120, 140);
        count++;
        delay(1000);
        esp_now_send(senderAddress, (u8_t *)&id_time, 2);
    }

    // Draw the page
    drawPage();
}

void changePage(s8_t change)
{
    page = (PAGES + page + change) % PAGES;
    preferences.putUChar("page", page);
}

void loop()
{
    M5Dial.update();
    auto t = M5Dial.Touch.getDetail();
    switch (t.state)
    {
    case 2: // Tap End
        t.x > 120 ? changePage(1) : changePage(-1);
        drawPage();
        break;
    case 6: // Hold End
        dark = !dark;
        M5Dial.Display.invertDisplay(dark);
        preferences.putBool("dark", dark);
        break;
    case 10: // Flick End
        t.x > 120 ? changePage(1) : changePage(-1);
        drawPage();
        break;
    case 14: // Drag End
        t.x > 120 ? changePage(1) : changePage(-1);
        drawPage();
        break;
    }
    u16_t newPosition = M5Dial.Encoder.read() / 4;
    s8_t change = newPosition - oldPosition;
    if (change != 0)
    {
        changePage(change);
        drawPage();
        oldPosition = newPosition;
    }
    if (render)
    {
        switch (page)
        {
        case (PAGE_INDICATORS):
            M5Dial.Display.setTextSize(2);
            switch (value)
            {
            case 0:
                M5Dial.Display.drawString("        ", 120, 125, &Orbitron_Light_32);
                break;
            case 5: // Harzards Low
                M5Dial.Display.drawString("   ><   ", 120, 125, &Orbitron_Light_32);
                break;
            case 10: // Hazards High
                M5Dial.Display.drawString(">><<", 120, 125, &Orbitron_Light_32);
                break;
            case 1: // Left Low
                M5Dial.Display.drawString("  <", 90, 125, &Orbitron_Light_32);
                break;
            case 2: // Left High
                M5Dial.Display.drawString("<<", 90, 125, &Orbitron_Light_32);
                break;
            case 4: // Right Low
                M5Dial.Display.drawString(">  ", 150, 125, &Orbitron_Light_32);
                break;
            case 8: // Right High
                M5Dial.Display.drawString(">>", 150, 125, &Orbitron_Light_32);
                break;
            }
            break;
        default: // Standard Metrics
            M5Dial.Display.setTextSize(2);
            if (decimal && (value < 2000) && (value > -1000))
            {
                M5Dial.Display.drawString("  " + String(value / 10) + "." + String(abs(value) % 10) + "  ", 120, 125, &FreeSansBold24pt7b);
            }
            else
            {
                M5Dial.Display.drawString("    " + String(value / 10) + "    ", 120, 125, &FreeSansBold24pt7b);
            }
            break;
        }
        render = false;
    }
}