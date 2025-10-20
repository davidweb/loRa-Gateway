#include "LoRaHandler.h"
#include "config.h"
#include "types.h"
#include "DeviceManager.h"
#include "helpers.h"
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
                    continue; // Skip this message
                }

                if (rxDoc[LORA_KEY_PAYLOAD].isNull() || rxDoc[LORA_KEY_CRC].isNull()) {
                    Serial.printf("LORA RX: Invalid message format. Msg: %s\n", rxStr.c_str());
                    continue;
                }

                String encryptedPayload = rxDoc[LORA_KEY_PAYLOAD];
                String decryptedPayload = decrypt_payload(encryptedPayload);

                if (decryptedPayload.length() == 0) {
                    Serial.println("LORA RX: Decryption failed!");
                    continue;
                }

                uint32_t receivedCrc = rxDoc[LORA_KEY_CRC];
                uint32_t calculatedCrc = calculateCRC32((const uint8_t*)decryptedPayload.c_str(), decryptedPayload.length());

                if (receivedCrc != calculatedCrc) {
                    Serial.printf("LORA RX: CRC mismatch! RX: %u, CALC: %u. Payload: %s\n", receivedCrc, calculatedCrc, decryptedPayload.c_str());
                    continue;
                }

                JsonDocument decryptedDoc;
                if (deserializeJson(decryptedDoc, decryptedPayload) != DeserializationError::Ok) {
                    Serial.printf("LORA RX: Decrypted payload JSON parsing failed! Payload: %s\n", decryptedPayload.c_str());
                    continue;
                }

                const char* type = decryptedDoc[LORA_KEY_TYPE];
                Serial.printf("LORA RX Decrypted: Type=%s\n", type);

                if (strcmp(type, LORA_MSG_TYPE_JOIN_REQUEST) == 0) {
                    int8_t newId = deviceManager.registerDevice(decryptedDoc[LORA_KEY_MAC], decryptedDoc[LORA_KEY_DEV_TYPE]);
                    if (newId > 0) {
                        JsonDocument responseDoc;
                        JsonObject p = responseDoc[LORA_KEY_PAYLOAD].to<JsonObject>();
                        p[LORA_KEY_TYPE] = LORA_MSG_TYPE_JOIN_ACCEPT;
                        p[LORA_KEY_NODE_ID] = newId;

                        String responsePayloadStr;
                        serializeJson(p, responsePayloadStr);

                        String encryptedResponse = encrypt_payload(responsePayloadStr);
                        txDoc.clear();
                        txDoc[LORA_KEY_PAYLOAD] = encryptedResponse;
                        txDoc[LORA_KEY_CRC] = calculateCRC32((const uint8_t*)responsePayloadStr.c_str(), responsePayloadStr.length());

                        String response;
                        serializeJson(txDoc, response);
                        radio.transmit(response);
                        Serial.printf("LORA TX -> JOIN_ACCEPT (encrypted) sent for Node %d\n", newId);

                        SystemEvent event = { NEW_DEVICE_REGISTERED, (uint8_t)newId };
                        xQueueSend(systemQueue, &event, 0);
                    }
                } else if (waitingForAck && strcmp(type, LORA_MSG_TYPE_ACK) == 0) {
                    uint16_t ackMsgId = decryptedDoc[LORA_KEY_MSG_ID];
                    if (ackMsgId == pendingAckCmd.msgId) {
                        Serial.printf("LORA ACK OK for msgId %d\n", ackMsgId);
                        waitingForAck = false;
                    }
                } else if (strcmp(type, LORA_MSG_TYPE_TELEMETRY) == 0) {
                    uint8_t nodeId = decryptedDoc[LORA_KEY_NODE_ID];
                    uint32_t msgCtr = decryptedDoc[LORA_KEY_MSG_COUNTER];

                    if (deviceManager.isDeviceRegistered(nodeId) && deviceManager.isValidMessageCounter(nodeId, msgCtr)) {
                        float rssi = radio.getRSSI();
                        float snr = radio.getSNR();
                        deviceManager.updateDeviceSignalInfo(nodeId, rssi, snr);

                        JsonObject data = decryptedDoc[LORA_KEY_DATA];
                        if (!data.isNull()) {
                            data["rssi"] = rssi;
                            data["snr"] = snr;
                        }

                        LoRaMessage msg;
                        msg.nodeId = nodeId;
                        serializeJson(decryptedDoc, msg.payload, sizeof(msg.payload));

                        if (xQueueSend(loraRxQueue, &msg, pdMS_TO_TICKS(10)) != pdPASS) {
                            Serial.println("LoRa RX Queue is full!");
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

    int plaintextLen = plaintext.length() + 1; // +1 for null terminator
    int paddedLen = (plaintextLen / 16 + (plaintextLen % 16 == 0 ? 0 : 1)) * 16;

    if (paddedLen > 256) {
        Serial.println("Error: Plaintext too long for encryption buffer!");
        return "";
    }

    byte paddedPlaintext[paddedLen];
    memset(paddedPlaintext, 0, paddedLen);
    memcpy(paddedPlaintext, plaintext.c_str(), plaintextLen);

    byte encrypted[paddedLen];
    aesLib.encrypt(paddedPlaintext, paddedLen, encrypted, key, sizeof(key), iv);

    char b64_output[base64_enc_len(paddedLen)];
    base64_encode(b64_output, (char*)encrypted, paddedLen);

    return String(b64_output);
}

String decrypt_payload(const String& b64_ciphertext) {
    byte key[16];
    byte iv[16];
    memcpy(key, LORA_SECRET_KEY, 16);
    memcpy(iv, LORA_AES_IV, 16);

    if (b64_ciphertext.length() > 342) { // 256 bytes padded to 272, then base64 to ~363. Safety margin.
        Serial.println("Error: Ciphertext too long for decryption buffer!");
        return "";
    }

    char b64_input[b64_ciphertext.length() + 1];
    b64_ciphertext.toCharArray(b64_input, sizeof(b64_input));

    int decodedLen = base64_dec_len(b64_input, sizeof(b64_input));
    if (decodedLen > 256) {
        Serial.println("Error: Decoded length exceeds buffer size!");
        return "";
    }

    byte decoded[decodedLen];
    base64_decode((char*)decoded, b64_input, sizeof(b64_input));

    byte decrypted[decodedLen];
    aesLib.decrypt(decoded, decodedLen, decrypted, key, sizeof(key), iv);

    // Remove PKCS7 padding
    int pad = decrypted[decodedLen - 1];
    if (pad > 0 && pad <= 16) {
        bool padding_ok = true;
        for (int i = 0; i < pad; i++) {
            if (decrypted[decodedLen - 1 - i] != pad) {
                padding_ok = false;
                break;
            }
        }
        if (padding_ok) {
            return String((char*)decrypted).substring(0, decodedLen - pad);
        }
    }

    return String((char*)decrypted);
}
