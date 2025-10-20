#include "LoRaHandler.h"
#include "config.h"
#include "types.h"
#include "DeviceManager.h"
#include <RadioLib.h>
#include <ArduinoJson.h>

extern SX1262 radio;
extern QueueHandle_t loraTxQueue;
extern QueueHandle_t loraRxQueue;
extern QueueHandle_t systemQueue;
extern SystemStatus systemStatus;

static TaskHandle_t loraTaskHandle = NULL;

static void IRAM_ATTR loraInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(loraTaskHandle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

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
    loraTaskHandle = xTaskGetCurrentTaskHandle();
    esp_task_wdt_add(NULL);
    Serial.println("LoRa Task started");
    StaticJsonDocument<256> txDoc;
    StaticJsonDocument<512> rxDoc;

    radio.startReceive();

    for (;;) {
        esp_task_wdt_reset();

        // Check for outgoing messages from other tasks
        LoRaTxCommand cmd;
        if (xQueueReceive(loraTxQueue, &cmd, 0) == pdPASS) {
            int state = radio.transmit(cmd.payload, strlen(cmd.payload));
            if (state == RADIOLIB_ERR_NONE) {
                Serial.printf("LORA TX -> Node %d: %s\n", cmd.targetNodeId, cmd.payload);
            } else {
                Serial.printf("LORA TX failed, code: %d\n", state);
            }
            // Go back to receive mode immediately after transmission
            radio.startReceive();
        }

        // Wait for a packet reception interrupt, with a timeout to remain responsive to TX commands
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50))) {
            String rxStr;
            int state = radio.readData(rxStr);

            if (state == RADIOLIB_ERR_NONE && rxStr.length() > 0) {
                systemStatus.lastLoRaRxTime = millis();
                DeserializationError error = deserializeJson(rxDoc, rxStr);

                if (error) {
                    Serial.printf("LORA RX: JSON parsing failed! Msg: %s\n", rxStr.c_str());
                } else {
                    JsonObject payload = rxDoc[LORA_KEY_PAYLOAD];
                    if (!payload.isNull()) {
                        uint32_t receivedCrc = rxDoc[LORA_KEY_CRC];
                        String payloadStr;
                        serializeJson(payload, payloadStr);
                        uint32_t calculatedCrc = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());

                        if (receivedCrc != calculatedCrc) {
                            Serial.printf("LORA RX: CRC mismatch! RX: %u, CALC: %u\n", receivedCrc, calculatedCrc);
                        } else {
                            Serial.printf("LORA RX: CRC OK. Payload: %s\n", payloadStr.c_str());
                            const char* type = payload[LORA_KEY_TYPE];

                            if (strcmp(type, LORA_MSG_TYPE_JOIN_REQUEST) == 0) {
                                if (strcmp(payload[LORA_KEY_SECRET], LORA_SECRET_KEY) == 0) {
                                    int8_t newId = deviceManager.registerDevice(payload[LORA_KEY_MAC], payload[LORA_KEY_DEV_TYPE]);
                                    if (newId > 0) {
                                        txDoc.clear();
                                        JsonObject p = txDoc.createNestedObject(LORA_KEY_PAYLOAD);
                                        p[LORA_KEY_TYPE] = LORA_MSG_TYPE_JOIN_ACCEPT;
                                        p[LORA_KEY_NODE_ID] = newId;
                                        String responsePayload; serializeJson(p, responsePayload);
                                        txDoc[LORA_KEY_CRC] = calculateCRC32((const uint8_t*)responsePayload.c_str(), responsePayload.length());
                                        String response; serializeJson(txDoc, response);

                                        radio.transmit(response);
                                        Serial.printf("LORA TX -> JOIN_ACCEPT sent for Node %d\n", newId);

                                        SystemEvent event = { NEW_DEVICE_REGISTERED, (uint8_t)newId };
                                        xQueueSend(systemQueue, &event, 0);
                                    }
                                }
                            } else if (strcmp(type, LORA_MSG_TYPE_TELEMETRY) == 0) {
                                uint8_t nodeId = payload[LORA_KEY_NODE_ID];
                                if (deviceManager.isDeviceRegistered(nodeId)) {
                                    deviceManager.updateDeviceLastSeen(nodeId);
                                    if (xQueueSend(loraRxQueue, &payload, pdMS_TO_TICKS(10)) != pdPASS) {
                                        Serial.println("LoRa RX Queue is full!");
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (state != RADIOLIB_ERR_RX_TIMEOUT && state != RADIOLIB_ERR_NONE) {
                Serial.printf("LORA RX failed, code: %d\n", state);
            }

            // Always go back to receive mode for the next packet
            radio.startReceive();
        }
    }
}
