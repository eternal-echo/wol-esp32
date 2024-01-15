#include <WiFi.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "config.h"

// MQTT 配置
const char* mqttHost = MQTT_HOST;
const int mqttPort = MQTT_PORT;
const char* mqttClientId = MQTT_CLIENT_ID;
const char* mqttUsername = MQTT_USERNAME;
const char* mqttPassword = MQTT_PASSWORD;

// 电脑的MAC地址
const char* MACAddresses[] = {MAC_ADDRESS1, MAC_ADDRESS2, MAC_ADDRESS3, MAC_ADDRESS4};

// 初始化UDP和WOL
WiFiUDP UDP;
WakeOnLan WOL(UDP);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// 连接到WiFi
void connectToWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

// 连接到MQTT服务器
void connectToMQTT() {
    mqttClient.setServer(mqttHost, mqttPort);
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");

        if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
            Serial.println("Connected to MQTT");
        } else {
            Serial.print("Failed with state ");
            Serial.println(mqttClient.state());
            Serial.println("Try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// // MQTT消息回调
// void mqttCallback(char* topic, byte* payload, unsigned int length) {
//     // 处理MQTT消息
//     Serial.print("Message arrived [");
//     Serial.print(topic);
//     Serial.print("] ");
//     for (int i = 0; i < length; i++) {
//         Serial.print((char)payload[i]);
//     }
//     Serial.println();
// }

// MQTT消息回调
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    // 创建一个足够大的缓冲区来存储消息内容
    char message[length + 1];
    for (int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0'; // Null-terminate the string
    Serial.println(message);

    // 解析JSON消息
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    // 提取 "params" 部分
    JsonObject params = doc["params"];
    if (params) {
        for (int i = 0; i < 4; i++) {
            String switchKey = "SocketSwitch_" + String(i + 1);
            if (params.containsKey(switchKey)) {
                bool switchState = params[switchKey];
                if (switchState) {
                    Serial.print("Waking up computer: ");
                    Serial.println(i + 1);
                    WOL.sendMagicPacket(MACAddresses[i]);
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    connectToWiFi();
    mqttClient.setCallback(mqttCallback);
    connectToMQTT();

    WOL.setRepeat(3, 100); // 重复发送WOL魔术包
}

void loop() {
    if (!mqttClient.connected()) {
        connectToMQTT();
    }
    mqttClient.loop();
}
