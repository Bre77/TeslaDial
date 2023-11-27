#include <Arduino.h>
#include <M5Dial.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

M5Canvas graph(&M5Dial.Display);

Preferences preferences;

u8_t senderAddress[] = {0xD4, 0xD4, 0xDA, 0x9D, 0xFD, 0xE4};
esp_now_peer_info_t peerInfo;

bool render = false;
int32_t oldPosition = 0;
int32_t value = INT32_MAX;
int32_t old_value = INT32_MAX;
int32_t high_value = 1000;
int32_t low_value = 0;
int32_t sum_value = 0;
int32_t sum_count = 0;

unsigned long next_value = ULONG_MAX;
unsigned long next_graph = ULONG_MAX;
uint8_t page = 0;
uint8_t brightness = 255;
static const uint8_t brightness_step = 16;
static const uint8_t brightness_min = 1;
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

#define GRAPH_HEIGHT 72
#define GRAPH_WIDTH 220
#define GRAPH_OFFSET 10
#define GRAPH_SPAN 15 * 1000 / 240
#define BRIGHTNESS_STEP = 16

#define PAGES 6

#define PAGE_TIME 0
#define PAGE_SPEED 1
#define PAGE_HV_BATTERY_POWER 2
#define PAGE_HV_BATTERY_VOLTAGE 3
#define PAGE_HV_BATTERY_CURRENT 203
#define PAGE_REAR_TORQUE 4
#define PAGE_REAR_POWER 5
#define PAGE_REAR_AMPS 206
#define PAGE_FRONT_TORQUE 6
#define PAGE_FRONT_POWER 7
#define PAGE_FRONT_AMPS 209
#define PAGE_CABIN_TEMP 210
#define PAGE_ACTUAL_TEMP 211
#define PAGE_OUTSIDE_TEMP 212

#define PAGE_TEST 214
#define PAGE_INDICATORS 103
#define PAGE_AC_LEFT 101
#define PAGE_AC_RIGHT 102

// Receive data
void OnDataRecv(const u8_t *mac, const u8_t *data, int len)
{
    /*if (millis() < next_value)
        return; // Rate Limit
    next_value = millis() + 99;*/
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
    case PAGE_TIME: // 7|32@0+ (1,0) [0|4294970000]
        value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
        // value = ((u32_t)data + 36000) % 86400;
        decimal = false;
        break;
    case PAGE_TEST:
        value = (value + 10) % 1024;
        decimal = true;
        break;
    }
    render = value != old_value;
    sum_value += value;
    sum_count++;

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
    M5Dial.Display.startWrite();
    M5Dial.Display.clear();
    M5Dial.Display.setTextSize(1);
    switch (page)
    {
    case PAGE_SPEED:
        esp_now_send(senderAddress, (u8_t *)&id_speed, 2);
        M5Dial.Display.drawString("Speed", 120, 50);
        high_value = 1200;
        low_value = 0;
        // M5Dial.Display.drawString("KM/H", 120, 180);
        break;
    case PAGE_AC_LEFT:
        esp_now_send(senderAddress, (u8_t *)&id_hvac_request, 2);
        M5Dial.Display.drawString("AC Left", 120, 50);
        // M5Dial.Display.drawString("Celsius", 120, 180);
        high_value = 300;
        low_value = 150;
        break;
    case PAGE_AC_RIGHT:
        esp_now_send(senderAddress, (u8_t *)&id_hvac_request, 2);
        M5Dial.Display.drawString("AC Right", 120, 50);
        // M5Dial.Display.drawString("Celsius", 120, 180);
        high_value = 300;
        low_value = 150;
        break;
    case PAGE_FRONT_POWER:
        esp_now_send(senderAddress, (u8_t *)&id_front_power, 2);
        M5Dial.Display.drawString("Front", 120, 20);
        M5Dial.Display.drawString("Power kW", 120, 50);
        // M5Dial.Display.drawString("kW", 120, 180);
        high_value = 5000;
        low_value = -5000;
        break;
    case PAGE_REAR_POWER:
        esp_now_send(senderAddress, (u8_t *)&id_rear_power, 2);
        M5Dial.Display.drawString("Rear", 120, 20);
        M5Dial.Display.drawString("Power  kW", 120, 50);
        // M5Dial.Display.drawString("kW", 120, 180);
        high_value = 5000;
        low_value = -5000;
        break;
    case PAGE_FRONT_TORQUE:
        esp_now_send(senderAddress, (u8_t *)&id_front_torque, 2);
        M5Dial.Display.drawString("Front", 120, 20);
        M5Dial.Display.drawString("Torque Nm", 120, 50);
        // M5Dial.Display.drawString("Nm", 120, 180);
        high_value = 5000;
        low_value = -5000;
        break;
    case PAGE_REAR_TORQUE:
        esp_now_send(senderAddress, (u8_t *)&id_rear_torque, 2);
        M5Dial.Display.drawString("Rear", 120, 20);
        M5Dial.Display.drawString("Torque Nm", 120, 50);
        // M5Dial.Display.drawString("Nm", 120, 180);
        high_value = 5000;
        low_value = -5000;
        break;
    case PAGE_FRONT_AMPS:
        esp_now_send(senderAddress, (u8_t *)&id_front_amps, 2);
        M5Dial.Display.drawString("Front", 120, 20);
        M5Dial.Display.drawString("Motor", 120, 50);
        // M5Dial.Display.drawString("Amps", 120, 180);
        high_value = 1000;
        low_value = -1000;
        break;
    case PAGE_REAR_AMPS:
        esp_now_send(senderAddress, (u8_t *)&id_rear_amps, 2);
        M5Dial.Display.drawString("Rear", 120, 20);
        M5Dial.Display.drawString("Motor", 120, 50);
        // M5Dial.Display.drawString("Amps", 120, 180);
        high_value = 1000;
        low_value = -1000;
        break;
    case PAGE_HV_BATTERY_POWER:
        esp_now_send(senderAddress, (u8_t *)&id_hv_battery, 2);
        M5Dial.Display.drawString("HV", 120, 20);
        M5Dial.Display.drawString("Power  kW", 120, 50);
        // M5Dial.Display.drawString("kW", 120, 180);
        high_value = 1000;
        low_value = -1000;
        break;
    case PAGE_HV_BATTERY_VOLTAGE:
        esp_now_send(senderAddress, (u8_t *)&id_hv_battery, 2);
        M5Dial.Display.drawString("HV", 120, 20);
        M5Dial.Display.drawString("Voltage", 120, 50);
        // M5Dial.Display.drawString("Volts", 120, 180);
        high_value = 4000;
        low_value = 3500;
        break;
    case PAGE_HV_BATTERY_CURRENT:
        esp_now_send(senderAddress, (u8_t *)&id_hv_battery, 2);
        M5Dial.Display.drawString("HV", 120, 20);
        M5Dial.Display.drawString("Current", 120, 50);
        // M5Dial.Display.drawString("Amps", 120, 180);
        high_value = 10000;
        low_value = -10000;
        break;
    case PAGE_INDICATORS:
        esp_now_send(senderAddress, (u8_t *)&id_lights, 2);
        M5Dial.Display.drawString("Indicators", 120, 50);
        break;
    case PAGE_CABIN_TEMP:
        esp_now_send(senderAddress, (u8_t *)&id_hvac_status, 4);
        M5Dial.Display.drawString("Cabin", 120, 50);
        M5Dial.Display.drawString("Temp", 120, 20);
        // M5Dial.Display.drawString("Celsius", 120, 180);
        high_value = 400;
        low_value = 0;
        break;
    case PAGE_ACTUAL_TEMP:
        esp_now_send(senderAddress, (u8_t *)&id_ths, 2);
        M5Dial.Display.drawString("Inside", 120, 50);
        M5Dial.Display.drawString("Temp", 120, 20);
        // M5Dial.Display.drawString("Celsius", 120, 180);
        high_value = 400;
        low_value = 0;
        break;
    case PAGE_OUTSIDE_TEMP:
        esp_now_send(senderAddress, (u8_t *)&id_front_sensor, 2);
        M5Dial.Display.drawString("Outside", 120, 50);
        M5Dial.Display.drawString("Temp", 120, 20);
        // M5Dial.Display.drawString("Celsius", 120, 180);
        high_value = 400;
        low_value = 0;
        break;
    case PAGE_TIME:
        esp_now_send(senderAddress, (u8_t *)&id_time, 2);
        M5Dial.Display.drawString("Time", 120, 50);
        break;
    case PAGE_TEST:
        esp_now_send(senderAddress, (u8_t *)&id_time, 2);
        M5Dial.Display.drawString("Test", 120, 50);
        high_value = 1000;
        low_value = 0;
        break;
    }
    M5Dial.Display.endWrite();
    value = INT16_MAX;
    sum_value = 0;
    sum_count = 0;
    next_value = 0;
    next_graph = 0;
    render = false;
}

void applyColor()
{
    if (dark)
    {
        M5Dial.Display.clear(TFT_BLACK);
        M5Dial.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        // graph.clear(TFT_BLACK);
    }
    else
    {
        M5Dial.Display.clear(TFT_WHITE);
        M5Dial.Display.setTextColor(TFT_BLACK, TFT_WHITE);
        // graph.clear(TFT_WHITE);
    }
}

void setup()
{
    // Setup WiFi for ESP Now
    WiFi.mode(WIFI_STA);

    // Setup persistant storage
    preferences.begin("dial", false);
    page = preferences.getUChar("page", 0) % PAGES;
    dark = preferences.getBool("dark", true);
    brightness = preferences.getUChar("brightness", 255);

    // Setup M5Dial
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);

    M5Dial.Display.setRotation(3);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setFont(&Orbitron_Light_24); // FreeSansBold12pt7b
    // M5Dial.Display.invertDisplay(dark);
    M5Dial.Display.setBrightness(brightness);
    graph.createSprite(GRAPH_WIDTH, GRAPH_HEIGHT);

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

    applyColor();

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
    graph.clear();
}

void changeBrightness(bool up)
{

    brightness = up ? (brightness << 1) + 1 : brightness >> 1;
    M5Dial.Display.setBrightness(brightness);
}

void loop()
{
    M5Dial.update();
    auto t = M5Dial.Touch.getDetail();
    if (t.state == 6)
    {
        dark = !dark;
        applyColor();
        drawPage();
    }
    else if (t.state % 4 == 2)
    {
        s8_t x = t.x - 120;
        s8_t y = t.y - 120;
        if (x <= -abs(y))
        {
            // LEFT
            changePage(-1);
            drawPage();
        }
        else if (x > abs(y))
        {
            // RIGHT
            changePage(1);
            drawPage();
        }
        else if (y <= -abs(x))
        {
            // UP
            changeBrightness(true);
        }
        else if (y > abs(x))
        {
            // DOWN
            changeBrightness(false);
        }
    }
    u16_t newPosition = M5Dial.Encoder.read() / 4;
    s8_t change = newPosition - oldPosition;
    if (change != 0)
    {
        changePage(change);
        drawPage();
        oldPosition = newPosition;
    }

    switch (page)
    {
    case (PAGE_INDICATORS):
        if (!render)
            break;
        M5Dial.Display.setTextSize(2);
        switch (value)
        {
        case 0:
            M5Dial.Display.drawString("        ", 120, 120, &Orbitron_Light_32);
            break;
        case 5: // Harzards Low
            M5Dial.Display.drawString("   ><   ", 120, 120, &Orbitron_Light_32);
            break;
        case 10: // Hazards High
            M5Dial.Display.drawString(">><<", 120, 120, &Orbitron_Light_32);
            break;
        case 1: // Left Low
            M5Dial.Display.drawString("  <", 90, 120, &Orbitron_Light_32);
            break;
        case 2: // Left High
            M5Dial.Display.drawString("<<", 90, 120, &Orbitron_Light_32);
            break;
        case 4: // Right Low
            M5Dial.Display.drawString(">  ", 150, 120, &Orbitron_Light_32);
            break;
        case 8: // Right High
            M5Dial.Display.drawString(">>", 150, 120, &Orbitron_Light_32);
            break;
        }
        break;
    case (PAGE_TIME):
    {
        if (!render)
            break;
        u8_t hours = (value / 3600 + 10) % 12;
        u8_t mins = (value / 60) % 60;
        u8_t secs = value % 60;
        M5Dial.Display.startWrite();
        M5Dial.Display.setTextSize(2);
        mins > 9 ? M5Dial.Display.drawString(String(hours) + ":" + String(mins), 120, 120, &FreeSansBold24pt7b)
                 : M5Dial.Display.drawString(String(hours) + ":0" + String(mins), 120, 120, &FreeSansBold24pt7b);
        M5Dial.Display.setTextSize(1);
        secs > 9 ? M5Dial.Display.drawString(String(secs), 120, 190, &FreeSansBold24pt7b)
                 : M5Dial.Display.drawString("0" + String(secs), 120, 190, &FreeSansBold24pt7b);
        M5Dial.Display.endWrite();
        break;
    }
    default: // Standard Metrics

        if (render)
        {
            render = false;
            M5Dial.Display.setTextSize(2);
            if (decimal && (value < 2000) && (value > -1000))
            {
                M5Dial.Display.drawString("  " + String(value / 10) + "." + String(abs(value) % 10) + "  ", 120, 120, &FreeSansBold24pt7b);
            }
            else
            {
                M5Dial.Display.drawString("    " + String(value / 10) + "    ", 120, 120, &FreeSansBold24pt7b);
            }
        }

        if ((millis() >= next_graph) && (value != INT16_MAX))
        {
            next_graph = millis() + GRAPH_SPAN;
            int graph_value = sum_count > 0 ? sum_value / sum_count : value;
            sum_count = 0;
            sum_value = 0;
            u8_t y = map(abs(graph_value), max(0, low_value), max(abs(low_value), high_value), 0, GRAPH_HEIGHT);
            graph.scroll(-1, 0);
            graph.writeFastVLine(GRAPH_WIDTH - 1, GRAPH_HEIGHT - y, y, low_value < 0 ? (graph_value < 0 ? TFT_GREEN : TFT_RED) : TFT_BLUE);
            M5Dial.Display.startWrite();
            // Fill graph area to clear previous graph
            M5Dial.Display.fillRect(GRAPH_OFFSET, 239 - GRAPH_HEIGHT, GRAPH_WIDTH, GRAPH_HEIGHT, dark ? TFT_BLACK : TFT_WHITE);
            graph.pushSprite(GRAPH_OFFSET, 239 - GRAPH_HEIGHT, TFT_BLACK);
            // M5Dial.Display.drawNumber(y, 160, 200, &Font0);
            M5Dial.Display.endWrite();
        }
        break;
    }
}