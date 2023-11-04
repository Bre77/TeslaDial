#include <Arduino.h>
#include <esp_now.h>
#include <M5Atom.h>
#include <WiFi.h>
// 50:02:91:92:94:D8

uint8_t senderAddress[] = {0x50, 0x02, 0x91, 0x92, 0x94, 0xD8};
esp_now_peer_info_t peerInfo;

typedef struct can_value
{
    char type;
    int value;
} can_value;

can_value receive;
can_value send;

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

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&data, incomingData, sizeof(data));
    M5.dis.drawpix(data.type, data.value);
    /*Serial.print("Bytes received: ");
    Serial.println(len);
    Serial.print("Type: ");
    Serial.println(int(data.type));
    Serial.print("Value: ");
    Serial.println(data.value);
    Serial.println();*/
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
    esp_err_t result = esp_now_send(senderAddress, (uint8_t *)&data, sizeof(data));

    if (result == ESP_OK)
    {
        Serial.println("Sent with success");
    }
    else
    {
        Serial.println("Error sending the data");
    }
    delay(1000);
}