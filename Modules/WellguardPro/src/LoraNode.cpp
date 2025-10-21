#include "LoraNode.h"
#include "config.h"
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

// Déclaration de l'objet radio global et de la fonction de contrôle de la pompe depuis main.cpp
extern SX1262 radio;
void setPumpState(bool state, bool fromLora = false);

Preferences preferences;

// Fonction de calcul du CRC32 - DOIT être identique à celle de la passerelle
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

void LoraNode::init() {
    loadConfig();
    int state = radio.begin(LORA_FREQ);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("Erreur fatale: Initialisation LoRa impossible, code %d\n", state);
        delay(5000);
        ESP.restart();
    }
}

bool LoraNode::isJoined() {
    return nodeId != 0;
}

void LoraNode::loadConfig() {
    preferences.begin(NVS_NAMESPACE, true); // Lecture seule
    nodeId = preferences.getUChar("nodeId", 0);
    preferences.end();
    Serial.printf("Node ID chargé depuis NVS : %d\n", nodeId);
}

void LoraNode::saveConfig() {
    preferences.begin(NVS_NAMESPACE, false); // Lecture/Ecriture
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
        if (rxDoc.containsKey("p") && strcmp(rxDoc["p"]["type"], "JOIN_ACCEPT") == 0) {
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

void LoraNode::run() {
    if (nodeId == 0) {
        performJoinRequest();
        vTaskDelay(pdMS_TO_TICKS(5000));
    } else {
        listenForCommands();
    }
}

void LoraNode::listenForCommands() {
    String response;
    int state = radio.receive(response, 1000); // Écouter pendant 1s de manière non bloquante
    if (state == RADIOLIB_ERR_NONE && response.length() > 0) {
        StaticJsonDocument<256> rxDoc;
        DeserializationError error = deserializeJson(rxDoc, response);
        if (error || !rxDoc.containsKey("p") || !rxDoc.containsKey("c")) return;

        JsonObject p = rxDoc["p"];
        String payloadStr; serializeJson(p, payloadStr);
        if (calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length()) != rxDoc["c"].as<uint32_t>()) {
             Serial.println("Commande LoRa reçue: CRC invalide !");
             return;
        }
        
        if (strcmp(p["type"], "CMD") == 0) {
            Serial.println("Commande LoRa valide reçue !");
            if (strcmp(p["method"], "setPump") == 0) {
                setPumpState(p["params"]["state"], true); // Appeler la fonction de contrôle
            }
        }
    }
}

void LoraNode::sendTelemetry(float temp, float humidity, float voltage, bool pressureOk) {
    if (!isJoined() || (millis() - lastTelemetryTime < TELEMETRY_INTERVAL_MS)) {
        return;
    }
    lastTelemetryTime = millis();

    Serial.println("Envoi de la télémétrie...");
    StaticJsonDocument<256> doc;
    JsonObject p = doc.createNestedObject("p");
    p["type"] = "TELEMETRY";
    p["nodeId"] = nodeId;
    
    JsonObject data = p.createNestedObject("data");
    data["temperature"] = temp;
    data["humidity"] = humidity;
    data["voltage"] = voltage;
    data["pressure_ok"] = pressureOk;
    
    String payloadStr; serializeJson(p, payloadStr);
    doc["c"] = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());
    String msg; serializeJson(doc, msg);
    
    radio.transmit(msg);
}
