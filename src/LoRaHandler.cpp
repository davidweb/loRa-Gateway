#include "LoRaHandler.h"
#include "config.h"
#include "types.h"
#include "DeviceManager.h"
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <AESLib.h>
#include <Base64.h>
#include <esp_task_wdt.h>

AESLib aesLib;

extern SX1262 radio;
extern QueueHandle_t loraTxQueue;
extern QueueHandle_t loraRxQueue;
extern QueueHandle_t systemQueue;
extern SystemStatus systemStatus;

static TaskHandle_t loraTaskHandle = NULL;

void IRAM_ATTR loraInterrupt() {
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
    JsonDocument txDoc;
    JsonDocument rxDoc;

    LoRaTxCommand pendingAckCmd;
    bool waitingForAck = false;
    uint8_t ackRetries = 0;
    unsigned long ackSentTime = 0;
    const uint8_t MAX_ACK_RETRIES = 3;
    const unsigned long ACK_TIMEOUT_MS = 5000;

    radio.startReceive();

    for (;;) {
        esp_task_wdt_reset();

        if (!waitingForAck) {
            LoRaTxCommand cmd;
            if (xQueueReceive(loraTxQueue, &cmd, 0) == pdPASS) {
                int state = radio.transmit(cmd.payload, strlen(cmd.payload));
                if (state == RADIOLIB_ERR_NONE) {
                    Serial.printf("LORA TX -> Node %d: %s\n", cmd.targetNodeId, cmd.payload);
                    if (cmd.requireAck) {
                        waitingForAck = true;
                        pendingAckCmd = cmd;
                        ackRetries = 0;
                        ackSentTime = millis();
                    }
                } else {
                    Serial.printf("LORA TX failed, code: %d\n", state);
                }
                radio.startReceive();
            }
        } else {
            if (millis() - ackSentTime > ACK_TIMEOUT_MS) {
                if (ackRetries < MAX_ACK_RETRIES) {
                    ackRetries++;
                    Serial.printf("LORA ACK TIMEOUT -> Retrying (%d/%d) for msgId %d\n", ackRetries, MAX_ACK_RETRIES, pendingAckCmd.msgId);
                    int state = radio.transmit(pendingAckCmd.payload, strlen(pendingAckCmd.payload));
                    if (state != RADIOLIB_ERR_NONE) {
                         Serial.printf("LORA TX (retry) failed, code: %d\n", state);
                    }
                    ackSentTime = millis();
                    radio.startReceive();
                } else {
                    Serial.printf("LORA ACK FAIL -> Max retries reached for msgId %d\n", pendingAckCmd.msgId);
                    waitingForAck = false;
                }
            }
        }

        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50))) {
            String rxStr;
            int state = radio.readData(rxStr);

            if (state == RADIOLIB_ERR_NONE && rxStr.length() > 0) {
                systemStatus.lastLoRaRxTime = millis();
                DeserializationError error = deserializeJson(rxDoc, rxStr);

                if (error) {
                    Serial.printf("LORA RX: JSON parsing failed! Msg: %s\n", rxStr.c_str());
                } else {
                    if (!rxDoc[LORA_KEY_PAYLOAD].isNull()) {
                        JsonObject payload = rxDoc[LORA_KEY_PAYLOAD];
                        uint32_t receivedCrc = rxDoc[LORA_KEY_CRC];
                        String payloadStr;
                        serializeJson(payload, payloadStr);
                        uint32_t calculatedCrc = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());

                        if (receivedCrc == calculatedCrc && strcmp(payload[LORA_KEY_TYPE], LORA_MSG_TYPE_JOIN_REQUEST) == 0) {
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
                        }
                    } else if (!rxDoc[LORA_KEY_DATA].isNull()) {
                        uint32_t receivedCrc = rxDoc[LORA_KEY_CRC];
                        String encryptedData = rxDoc[LORA_KEY_DATA];
                        uint32_t calculatedCrc = calculateCRC32((const uint8_t*)encryptedData.c_str(), encryptedData.length());

                        if(receivedCrc != calculatedCrc){
                            Serial.printf("LORA RX: CRC mismatch! RX: %u, CALC: %u\n", receivedCrc, calculatedCrc);
                        } else {
                            String decryptedStr = decrypt_payload(encryptedData);
                            JsonDocument decryptedDoc;
                            deserializeJson(decryptedDoc, decryptedStr);

                            const char* type = decryptedDoc[LORA_KEY_TYPE];
                            Serial.printf("LORA RX Decrypted: Type=%s\n", type);

                            if (waitingForAck && strcmp(type, LORA_MSG_TYPE_ACK) == 0) {
                                uint16_t ackMsgId = decryptedDoc[LORA_KEY_MSG_ID];
                                if (ackMsgId == pendingAckCmd.msgId) {
                                    Serial.printf("LORA ACK OK for msgId %d\n", ackMsgId);
                                    waitingForAck = false;
                                }
                            } else if (strcmp(type, LORA_MSG_TYPE_TELEMETRY) == 0) {
                                uint8_t nodeId = decryptedDoc[LORA_KEY_NODE_ID];
                                if (deviceManager.isDeviceRegistered(nodeId)) {
                                    float rssi = radio.getRSSI();
                                    float snr = radio.getSNR();
                                    deviceManager.updateDeviceSignalInfo(nodeId, rssi, snr);

                                    JsonObject data = decryptedDoc[LORA_KEY_DATA];
                                    if (!data.isNull()) {
                                        data["rssi"] = rssi;
                                        data["snr"] = snr;
                                    }

                                    if (xQueueSend(loraRxQueue, &decryptedDoc, pdMS_TO_TICKS(10)) != pdPASS) {
                                        Serial.println("LoRa RX Queue is full!");
                                    }
                                }
                            } else if (strcmp(type, LORA_MSG_TYPE_CMD) == 0) {
                                Serial.printf("LoRa RX: Received CMD: %s. Acting on it is not implemented yet.\n", decryptedStr.c_str());
                            }
                        }
                    }
                }
            } else if (state != RADIOLIB_ERR_RX_TIMEOUT && state != RADIOLIB_ERR_NONE) {
                Serial.printf("LORA RX failed, code: %d\n", state);
            }
            radio.startReceive();
        }
    }
}

String encrypt_payload(const String& plaintext) {
    byte key[16];
    byte iv[16];
    memcpy(key, LORA_SECRET_KEY, 16);
    memcpy(iv, LORA_AES_IV, 16);

    int plaintextLen = plaintext.length() + 1;
    int paddedLen = (plaintextLen / 16 + 1) * 16;
    byte *paddedPlaintext = new byte[paddedLen];
    memset(paddedPlaintext, 0, paddedLen);
    memcpy(paddedPlaintext, plaintext.c_str(), plaintextLen);

    byte *encrypted = new byte[paddedLen];
    aesLib.encrypt(paddedPlaintext, paddedLen, encrypted, key, sizeof(key), iv);

    char b64_output[base64_enc_len(paddedLen)];
    base64_encode(b64_output, (char*)encrypted, paddedLen);
    String b64Str(b64_output);

    delete[] paddedPlaintext;
    delete[] encrypted;
    return b64Str;
}

String decrypt_payload(const String& b64_ciphertext) {
    byte key[16];
    byte iv[16];
    memcpy(key, LORA_SECRET_KEY, 16);
    memcpy(iv, LORA_AES_IV, 16);

    char b64_input[b64_ciphertext.length() + 1];
    b64_ciphertext.toCharArray(b64_input, sizeof(b64_input));

    int decodedLen = base64_dec_len(b64_input, sizeof(b64_input));
    byte *decoded = new byte[decodedLen];
    base64_decode((char*)decoded, b64_input, sizeof(b64_input));

    byte *decrypted = new byte[decodedLen];
    aesLib.decrypt(decoded, decodedLen, decrypted, key, sizeof(key), iv);

    String plaintext = String((char*)decrypted);

    delete[] decoded;
    delete[] decrypted;
    return plaintext;
}
