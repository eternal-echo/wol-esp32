#include "config.h"

#include <ArduinoOTA.h>
// #include <WiFi.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// #define WOL_DEBUG

#ifdef WOL_DEBUG
#define WOL_DEBUG_PRINT Serial.print
#define WOL_DEBUG_PRINTLN Serial.println
#define WOL_DEBUG_PRINTF Serial.printf
#else
#define WOL_DEBUG_PRINT
#define WOL_DEBUG_PRINTLN
#define WOL_DEBUG_PRINTF
#endif

// MQTT 配置
const char* mqttHost = MQTT_HOST;
const int mqttPort = MQTT_PORT;
const char* mqttClientId = MQTT_CLIENT_ID;
const char* mqttUsername = MQTT_USERNAME;
const char* mqttPassword = MQTT_PASSWORD;

// 电脑的MAC地址
const char* MACAddresses[] = {MAC_ADDRESS1, MAC_ADDRESS2, MAC_ADDRESS3, MAC_ADDRESS4, MAC_ADDRESS5};

// 初始化UDP和WOL
WiFiUDP UDP;
WakeOnLan WOL(UDP);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// OTA
void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    WOL_DEBUG_PRINTLN("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    WOL_DEBUG_PRINTLN("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    WOL_DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    WOL_DEBUG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      WOL_DEBUG_PRINTLN("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      WOL_DEBUG_PRINTLN("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      WOL_DEBUG_PRINTLN("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      WOL_DEBUG_PRINTLN("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      WOL_DEBUG_PRINTLN("End Failed");
    }
  });
  ArduinoOTA.begin();
}

// 连接到WiFi
void connectToWiFi() {
  int wifiStatus = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);

  do {
    delay(500);
    wifiStatus = WiFi.status(); // 更新WiFi状态
    WOL_DEBUG_PRINT(wifiStatus); // 输出当前WiFi状态
  } while (wifiStatus != WL_CONNECTED);
  WOL_DEBUG_PRINTLN("");
  WOL_DEBUG_PRINTLN("WiFi connected");
  WOL_DEBUG_PRINTLN("IP address: ");
  WOL_DEBUG_PRINTLN(WiFi.localIP());
  setupOTA();
  WOL_DEBUG_PRINTLN("OTA initialized");
}

// 连接到MQTT服务器
void connectToMQTT() {
    mqttClient.setServer(mqttHost, mqttPort);
    while (!mqttClient.connected()) {
        WOL_DEBUG_PRINT("Attempting MQTT connection...");

        if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
            WOL_DEBUG_PRINTLN("Connected to MQTT");
        } else {
            WOL_DEBUG_PRINT("Failed with state ");
            WOL_DEBUG_PRINTLN(mqttClient.state());
            WOL_DEBUG_PRINTLN("Try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// MQTT消息回调
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    WOL_DEBUG_PRINT("Message arrived [");
    WOL_DEBUG_PRINT(topic);
    WOL_DEBUG_PRINT("] ");

    // 创建一个足够大的缓冲区来存储消息内容
    char message[length + 1];
    for (int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0'; // Null-terminate the string
    WOL_DEBUG_PRINTLN(message);

    // 解析JSON消息
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        WOL_DEBUG_PRINT("deserializeJson() failed: ");
        WOL_DEBUG_PRINTLN(error.c_str());
        return;
    }

    // 提取 "params" 部分
    JsonObject params = doc["params"];
    if (params) {
        for (int i = 0; i < MAC_ADDRESS_COUNT; i++) {
            String switchKey = "SocketSwitch_" + String(i + 1);
            if (params.containsKey(switchKey)) {
                bool switchState = params[switchKey];
                if (switchState) {
                    WOL_DEBUG_PRINT("Waking up computer: ");
                    WOL_DEBUG_PRINTLN(i + 1);
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
    ArduinoOTA.handle();
}
