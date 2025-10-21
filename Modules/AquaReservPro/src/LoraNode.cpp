#include "LoraNode.h"
#include "config.h"
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

extern SX1262 radio;
Preferences preferences;

// Fonction de calcul du CRC32 (doit être identique à celle de la passerelle)
uint32_t calculateCRC32(const uint8_t *data, size_t length); 

void LoraNode::init() {
    loadConfig();
    int state = radio.begin(LORA_FREQ);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("LoRa init failed, code %d\n", state);
        ESP.restart();
    }
}

void LoraNode::run() {
    if (nodeId == 0) {
        performJoinRequest();
        vTaskDelay(pdMS_TO_TICKS(5000)); // Attendre 5s avant de réessayer
    }
    // Si nous sommes joints, la tâche des capteurs déclenchera l'envoi de télémétrie.
}

bool LoraNode::isJoined() {
    return nodeId != 0;
}

void LoraNode::loadConfig() {
    preferences.begin(NVS_NAMESPACE, false);
    nodeId = preferences.getUChar("nodeId", 0);
    preferences.end();
    Serial.printf("Node ID chargé depuis NVS : %d\n", nodeId);
}

void LoraNode::saveConfig() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putUChar("nodeId", nodeId);
    preferences.end();
    Serial.printf("Node ID %d sauvegardé en NVS\n", nodeId);
}

void LoraNode::performJoinRequest() {
    Serial.println("Envoi de la demande d'adhésion (JOIN_REQUEST)...");
    StaticJsonDocument<256> doc;
    JsonObject p = doc.createNestedObject("p");
    p["type"] = "JOIN_REQUEST";
    p["mac"] = WiFi.macAddress();
    p["secret"] = LORA_SECRET_KEY;
    p["devType"] = DEVICE_TYPE;
    
    String payloadStr; serializeJson(p, payloadStr);
    doc["c"] = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());
    String msg; serializeJson(doc, msg);

    radio.transmit(msg);

    // Écouter la réponse
    String response;
    int state = radio.receive(response, 5000); // Écouter pendant 5 secondes
    if (state == RADIOLIB_ERR_NONE && response.length() > 0) {
        StaticJsonDocument<128> rxDoc;
        deserializeJson(rxDoc, response);
        // Ici, on ne vérifie pas le CRC pour la simplicité, mais en prod on le ferait
        if (strcmp(rxDoc["p"]["type"], "JOIN_ACCEPT") == 0) {
            nodeId = rxDoc["p"]["nodeId"];
            if(nodeId > 0) {
                Serial.printf("Adhésion réussie ! Node ID attribué : %d\n", nodeId);
                saveConfig();
            }
        }
    } else {
        Serial.println("Pas de réponse à la demande d'adhésion.");
    }
}

void LoraNode::sendTelemetry(bool isFull) {
    if (!isJoined()) return;

    Serial.printf("Envoi de la télémétrie : isFull = %d\n", isFull);
    StaticJsonDocument<256> doc;
    JsonObject p = doc.createNestedObject("p");
    p["type"] = "TELEMETRY";
    p["nodeId"] = nodeId;
    
    JsonObject data = p.createNestedObject("data");
    data["level_full"] = isFull;
    data["voltage"] = 3.3; // Exemple de valeur statique
    
    String payloadStr; serializeJson(p, payloadStr);
    doc["c"] = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());
    String msg; serializeJson(doc, msg);
    
    radio.transmit(msg);
}

// Dupliquer la fonction CRC32 de la passerelle ici
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
