#include "MqttHandler.h"
#include "config.h"
#include "types.h"
#include "DeviceManager.h"
#include "LoRaHandler.h" // Pour les fonctions de chiffrement
#include "helpers.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern QueueHandle_t loraTxQueue;
extern QueueHandle_t loraRxQueue;
extern QueueHandle_t systemQueue;
extern SystemStatus systemStatus;

void mqttCallback(char* topic, byte* payload, unsigned int length);

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;
    systemStatus.wifi = WIFI_CONNECTING;
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectMqtt() {
    if (mqttClient.connected()) return;
    systemStatus.mqtt = GW_MQTT_CONNECTING;
    Serial.println("Connecting to MQTT broker...");

    if (mqttClient.connect("Gateway_HeltecV3", TB_GATEWAY_TOKEN, NULL)) {
        systemStatus.mqtt = GW_MQTT_CONNECTED;
        Serial.println("MQTT Connected.");
        mqttClient.subscribe(TB_RPC_TOPIC);

        JsonDocument doc;
        JsonArray devices = doc.to<JsonArray>();
        deviceManager.getAllActiveDeviceNames(devices);
        for(JsonVariant deviceName : devices) {
            char payloadBuffer[64];
            snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"device\":\"%s\"}", deviceName.as<const char*>());
            mqttClient.publish(TB_CONNECT_TOPIC, payloadBuffer);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    } else {
        systemStatus.mqtt = GW_MQTT_DISCONNECTED;
        Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state());
    }
}

void taskMqttHandler(void *pvParameters) {
    esp_task_wdt_add(NULL);
    Serial.println("MQTT Task started");
    mqttClient.setServer(TB_SERVER, TB_PORT);
    mqttClient.setCallback(mqttCallback);

    StaticJsonDocument<256> telemetryDoc;
    char mqttPayload[384];
    unsigned long lastWifiAttempt = 0;
    unsigned long lastMqttAttempt = 0;

    for (;;) {
        esp_task_wdt_reset();

        if (WiFi.status() != WL_CONNECTED) {
            systemStatus.wifi = WIFI_DISCONNECTED;
            if (millis() - lastWifiAttempt > WIFI_RECONNECT_INTERVAL_MS) {
                connectWiFi();
                lastWifiAttempt = millis();
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        systemStatus.wifi = WIFI_CONNECTED;

        if (!mqttClient.connected()) {
            if (millis() - lastMqttAttempt > MQTT_RECONNECT_INTERVAL_MS) {
                connectMqtt();
                lastMqttAttempt = millis();
            }
        }
        
        mqttClient.loop();

        SystemEvent event;
        if (xQueueReceive(systemQueue, &event, 0) == pdPASS) {
            if (event.type == NEW_DEVICE_REGISTERED) {
                const char* deviceName = deviceManager.getDeviceName(event.nodeId);
                char payloadBuffer[64];
                snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"device\":\"%s\"}", deviceName);
                mqttClient.publish(TB_CONNECT_TOPIC, payloadBuffer);
            }
        }

        LoRaMessage rxMsg;
        if (xQueueReceive(loraRxQueue, &rxMsg, 0) == pdPASS) {
            StaticJsonDocument<256> telemetryDoc;
            deserializeJson(telemetryDoc, rxMsg.payload);

            const char* deviceName = deviceManager.getDeviceName(rxMsg.nodeId);
            JsonObject data = telemetryDoc[LORA_KEY_DATA];
            String dataStr;
            serializeJson(data, dataStr);

            snprintf(mqttPayload, sizeof(mqttPayload), "{\"%s\":[{\"ts\":%lu, \"values\":%s}]}",
                deviceName, millis(), dataStr.c_str());

            if (!mqttClient.publish(TB_TELEMETRY_TOPIC, mqttPayload)) {
                Serial.println("MQTT Publish failed!");
            } else {
                Serial.printf("MQTT TX: %s\n", mqttPayload);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    static uint16_t msgIdCounter = 0;
    Serial.printf("MQTT RX: [%s]\n", topic);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.printf("MQTT RX: JSON parsing failed: %s\n", error.c_str());
        return;
    }

    const char* deviceName = doc["device"]; 
    JsonObject data = doc["data"];
    if (!deviceName || data.isNull()) return;

    uint8_t targetNodeId = deviceManager.findNodeIdByName(deviceName);

    if (targetNodeId > 0) {
        LoRaTxCommand cmd;
        cmd.targetNodeId = targetNodeId;
        cmd.msgId = ++msgIdCounter;
        cmd.requireAck = true;

        JsonDocument plaintextDoc;
        plaintextDoc[LORA_KEY_TYPE] = LORA_MSG_TYPE_CMD;
        plaintextDoc[LORA_KEY_MSG_ID] = cmd.msgId;
        plaintextDoc[LORA_KEY_METHOD] = data[LORA_KEY_METHOD];
        plaintextDoc[LORA_KEY_PARAMS] = data[LORA_KEY_PARAMS];
        String plaintextStr;
        serializeJson(plaintextDoc, plaintextStr);

        String encryptedPayload = encrypt_payload(plaintextStr);

        JsonDocument loraDoc;
        loraDoc[LORA_KEY_PAYLOAD] = encryptedPayload;
        loraDoc[LORA_KEY_CRC] = calculateCRC32((const uint8_t*)plaintextStr.c_str(), plaintextStr.length());
        
        serializeJson(loraDoc, cmd.payload, sizeof(cmd.payload));

        if (xQueueSend(loraTxQueue, &cmd, pdMS_TO_TICKS(10)) != pdPASS) {
            Serial.println("LoRa TX Queue is full!");
        }
    } else {
        Serial.printf("MQTT RX: Command for unknown device '%s'\n", deviceName);
    }
}
