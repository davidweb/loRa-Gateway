#include "LoraNode.h"
#include "config.h"
#include "credentials.h"
#include "helpers.h"
#include "Base64.h"
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <AESLib.h>

extern SX1262 radio;
extern void setPumpState(bool state, bool fromLora);
Preferences preferences;
AESLib aesLib;

// Tampons pour le chiffrement/déchiffrement
byte aes_key[16];
byte aes_iv[16];
byte encrypted[256];
byte decrypted[256];


void LoraNode::init() {
    memcpy(aes_key, LORA_SECRET_KEY, 16);
    memcpy(aes_iv, LORA_AES_IV, 16);

    loadConfig();

    Serial.print(F("[LORA] Initializing... "));
    int state = radio.begin(LORA_FREQ);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("failed, code %d\n", state);
        ESP.restart();
    }
    Serial.println(F("success!"));
}

void LoraNode::run() {
    if (nodeId == 0) {
        if (millis() - lastJoinAttempt > 10000) { // Tenter de joindre toutes les 10s
            lastJoinAttempt = millis();
            performJoinRequest();
        }
    } else {
        listenForCommands();
    }
}

bool LoraNode::isJoined() {
    return nodeId != 0;
}

void LoraNode::loadConfig() {
    preferences.begin(NVS_NAMESPACE, false);
    nodeId = preferences.getUChar("nodeId", 0);
    msgCounter = preferences.getUInt("msgCtr", 0);
    preferences.end();
    Serial.printf("[NVS] Node ID: %d, Msg Counter: %u\n", nodeId, msgCounter);
}

void LoraNode::saveConfig() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putUChar("nodeId", nodeId);
    preferences.putUInt("msgCtr", msgCounter);
    preferences.end();
    Serial.printf("[NVS] Config saved. Node ID: %d, Msg Counter: %u\n", nodeId, msgCounter);
}

void LoraNode::performJoinRequest() {
    Serial.println(F("[LORA] Sending JOIN_REQUEST..."));
    
    StaticJsonDocument<200> doc;
    doc["type"] = "JOIN_REQUEST";
    doc["mac"] = WiFi.macAddress();
    doc["devType"] = DEVICE_TYPE;

    String payloadStr;
    serializeJson(doc, payloadStr);

    String encryptedPayload = encryptPayload(payloadStr);

    StaticJsonDocument<256> finalDoc;
    finalDoc["p"] = encryptedPayload;
    finalDoc["c"] = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());

    String msg;
    serializeJson(finalDoc, msg);

    int state = radio.transmit(msg);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Transmit failed, code %d\n", state);
        return;
    }

    // Écouter la réponse
    String response;
    state = radio.receive(response, 5000);
    if (state == RADIOLIB_ERR_NONE && response.length() > 0) {
        StaticJsonDocument<256> rxDoc;
        if (deserializeJson(rxDoc, response) == DeserializationError::Ok && rxDoc.containsKey("p")) {
            String decrypted = decryptPayload(rxDoc["p"]);
            if (decrypted.length() > 0) {
                StaticJsonDocument<128> joinAcceptDoc;
                if (deserializeJson(joinAcceptDoc, decrypted) == DeserializationError::Ok &&
                    joinAcceptDoc["type"] == "JOIN_ACCEPT") {

                    nodeId = joinAcceptDoc["nodeId"];
                    if (nodeId > 0) {
                        msgCounter = 0; // Réinitialiser le compteur après un join réussi
                        saveConfig();
                        Serial.printf("[LORA] Join successful! Assigned Node ID: %d\n", nodeId);
                    }
                }
            }
        }
    } else {
        Serial.println(F("[LORA] No response to JOIN_REQUEST."));
    }
}

void LoraNode::listenForCommands() {
    String response;
    int state = radio.receive(response, 1000); // Écoute non bloquante
    if (state == RADIOLIB_ERR_NONE && response.length() > 0) {
        StaticJsonDocument<384> rxDoc;
        if (deserializeJson(rxDoc, response) != DeserializationError::Ok || !rxDoc.containsKey("p")) return;

        String decrypted = decryptPayload(rxDoc["p"]);
        if (decrypted.length() == 0) return;

        StaticJsonDocument<256> cmdDoc;
        if (deserializeJson(cmdDoc, decrypted) != DeserializationError::Ok) return;

        if (cmdDoc["type"] == "CMD" && cmdDoc["nodeId"] == nodeId) {
            Serial.println("[LORA] Received CMD");
            if (cmdDoc["method"] == "setPump") {
                setPumpState(cmdDoc["params"]["state"], true);
                if (cmdDoc.containsKey("msgId")) {
                    sendAck(cmdDoc["msgId"]);
                }
            }
        }
    }
}

void LoraNode::sendAck(uint16_t msgId) {
    msgCounter++;
    StaticJsonDocument<128> doc;
    doc["type"] = "ACK";
    doc["nodeId"] = nodeId;
    doc["msgId"] = msgId;
    doc["msgCtr"] = msgCounter;

    String payloadStr;
    serializeJson(doc, payloadStr);

    String encryptedPayload = encryptPayload(payloadStr);

    StaticJsonDocument<256> finalDoc;
    finalDoc["p"] = encryptedPayload;
    finalDoc["c"] = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());

    String msg;
    serializeJson(finalDoc, msg);

    Serial.printf("[LORA] Sending ACK for msgId %d\n", msgId);
    if (radio.transmit(msg) == RADIOLIB_ERR_NONE) {
        saveConfig();
    } else {
        msgCounter--;
    }
}

void LoraNode::sendTelemetry(float temp, float humidity, float voltage, bool pressureOk) {
    if (!isJoined() || (millis() - lastTelemetryTime < TELEMETRY_INTERVAL_MS)) {
        return;
    }
    lastTelemetryTime = millis();
    msgCounter++;

    StaticJsonDocument<256> doc;
    doc["type"] = "TELEMETRY";
    doc["nodeId"] = nodeId;
    doc["msgCtr"] = msgCounter;

    JsonObject data = doc.createNestedObject("data");
    data["temperature"] = temp;
    data["humidity"] = humidity;
    data["voltage"] = voltage;
    data["pressure_ok"] = pressureOk;

    String payloadStr;
    serializeJson(doc, payloadStr);
    
    String encryptedPayload = encryptPayload(payloadStr);
    
    StaticJsonDocument<384> finalDoc;
    finalDoc["p"] = encryptedPayload;
    finalDoc["c"] = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());

    String msg;
    serializeJson(finalDoc, msg);

    Serial.printf("[LORA] Sending TELEMETRY (msgCtr: %u)...\n", msgCounter);
    if (radio.transmit(msg) == RADIOLIB_ERR_NONE) {
        saveConfig();
    } else {
        msgCounter--;
    }
}

String LoraNode::encryptPayload(const String& plaintext) {
    int plaintextLen = plaintext.length();
    int paddedLen = (plaintextLen / 16 + 1) * 16;
    byte pad = paddedLen - plaintextLen;

    byte paddedPlaintext[paddedLen];
    memcpy(paddedPlaintext, plaintext.c_str(), plaintextLen);
    for (int i = plaintextLen; i < paddedLen; i++) {
        paddedPlaintext[i] = pad;
    }

    aesLib.encrypt(paddedPlaintext, paddedLen, encrypted, aes_key, sizeof(aes_key), aes_iv);

    return Base64::encode(encrypted, paddedLen);
}

String LoraNode::decryptPayload(const String& b64_ciphertext) {
    String decoded_string = Base64::decode(b64_ciphertext);

    aesLib.decrypt((byte*)decoded_string.c_str(), decoded_string.length(), decrypted, aes_key, sizeof(aes_key), aes_iv);

    // Supprimer le padding PKCS7
    int pad = decrypted[decoded_string.length() - 1];
    if (pad > 0 && pad <= 16) {
        return String((char*)decrypted).substring(0, decoded_string.length() - pad);
    }
    return String((char*)decrypted);
}
