#include "LoRaHandler.h"
#include "config.h"
#include "types.h"
#include "DeviceManager.h"
#include <RadioLib.h>
#include <ArduinoJson.h>

extern SX1262 radio;
extern QueueHandle_t loraTxQueue;
extern QueueHandle_t loraRxQueue;
extern SystemStatus systemStatus;

uint32_t calculateCRC32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xffffffff;
    while (length--) {
        uint8_t c = *data++;
        for (uint32_t i = 0x80; i > 0; i >>= 1) {
            bool bit = crc & 0x80000000;
            if (c & i) { bit = !bit; }
            crc <<= 1;
            if (bit) { crc ^= 0x04c11db7; }
        }
    }
    return crc;
}

void taskLoRaHandler(void *pvParameters) {
    esp_task_wdt_add(NULL);
    Serial.println("LoRa Task started");
    StaticJsonDocument<256> txDoc;
    StaticJsonDocument<512> rxDoc;

    for (;;) {
        esp_task_wdt_reset();

        LoRaTxCommand cmd;
        if (xQueueReceive(loraTxQueue, &cmd, 0) == pdPASS) {
            int state = radio.transmit(cmd.payload, strlen(cmd.payload));
             if (state == RADIOLIB_ERR_NONE) {
                Serial.printf("LORA TX -> Node %d: %s\n", cmd.targetNodeId, cmd.payload);
            } else {
                Serial.printf("LORA TX failed, code: %d\n", state);
            }
        }

        String rxStr;
        int state = radio.receive(rxStr, 100);

        if (state == RADIOLIB_ERR_NONE && rxStr.length() > 0) {
            systemStatus.lastLoRaRxTime = millis();
            DeserializationError error = deserializeJson(rxDoc, rxStr);

            if (error) {
                Serial.printf("LORA RX: JSON parsing failed! Msg: %s\n", rxStr.c_str());
                continue;
            }

            JsonObject payload = rxDoc["p"];
            if (payload.isNull()) continue;

            uint32_t receivedCrc = rxDoc["c"];
            String payloadStr;
            serializeJson(payload, payloadStr);
            uint32_t calculatedCrc = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());

            if (receivedCrc != calculatedCrc) {
                Serial.printf("LORA RX: CRC mismatch! RX: %u, CALC: %u\n", receivedCrc, calculatedCrc);
                continue;
            }
            Serial.printf("LORA RX: CRC OK. Payload: %s\n", payloadStr.c_str());

            const char* type = payload["type"];

            if (strcmp(type, "JOIN_REQUEST") == 0) {
                if (strcmp(payload["secret"], LORA_SECRET_KEY) == 0) {
                    int8_t newId = deviceManager.registerDevice(payload["mac"], payload["devType"]);

                    if (newId > 0) {
                        txDoc.clear();
                        JsonObject p = txDoc.createNestedObject("p");
                        p["type"] = "JOIN_ACCEPT";
                        p["nodeId"] = newId;
                        
                        String responsePayload; serializeJson(p, responsePayload);
                        txDoc["c"] = calculateCRC32((const uint8_t*)responsePayload.c_str(), responsePayload.length());
                        
                        String response; serializeJson(txDoc, response);
                        radio.transmit(response);
                        Serial.printf("LORA TX -> JOIN_ACCEPT sent for Node %d\n", newId);
                        systemStatus.mqttNeedsReconnect = true;
                    }
                }
            } else if (strcmp(type, "TELEMETRY") == 0) {
                uint8_t nodeId = payload["nodeId"];
                if (deviceManager.isDeviceRegistered(nodeId)) {
                    deviceManager.updateDeviceLastSeen(nodeId);
                    if (xQueueSend(loraRxQueue, &payload, pdMS_TO_TICKS(10)) != pdPASS) {
                         Serial.println("LoRa RX Queue is full!");
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
