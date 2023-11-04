#include <Arduino.h>
#include <M5Atom.h>
#include <esp_now.h>
#include <WiFi.h>
// 50:02:91:92:94:D8

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

typedef struct can_value
{
    byte id;
    int value;
} can_value;

// Create a struct_message called myData
can_value data_send;
can_value data_receive;

void error()
{
    for (int i = 0; i < 50; i++)
    {
        M5.dis.drawpix(0, 0xff0000);
        delay(100);
        M5.dis.drawpix(0, 0x000000);
        delay(100);
    }
    ESP.restart();
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    memcpy(&data_receive, data, 3);
    Serial.print("Bytes received: ");
    Serial.println(data_len);
    Serial.print("Data received: ");
    Serial.println(data_receive.value);
    Serial.print("Data received: ");
    Serial.println(data_receive.type);
}

void setup()
{
    // put your setup code here, to run once:
    M5.begin(
        true,  // SerialEnable
        false, // I2CEnable
        true   // DisplayEnable
    );
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESPNow Init Failed");
        error();
    }
    Serial.println("ESPNow Init Success");

    esp_now_register_recv_cb(OnDataRecv);

    // Add broadcast peer
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        error();
    }
    Serial.println("Added peer");
    Serial.println(sizeof(&data_send));
}

void loop()
{
    data_send.id = random(0, 25);
    data_send.value = random(0, 32767);
    if (esp_now_send(broadcastAddress, (uint8_t *)&data_send, sizeof(data_send)) == ESP_OK)
    {
        M5.dis.drawpix(0, data_send.value);
    }
    else
    {
        Serial.println("Failed to send");
    }
    delay(50);
}