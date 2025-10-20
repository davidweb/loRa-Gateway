#pragma once
#include <Arduino.h>

// Énumération pour l'état de la connexion WiFi
enum WiFiStatus {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED
};

// Énumération pour l'état de la connexion MQTT
enum MqttStatus {
    MQTT_DISCONNECTED,
    MQTT_CONNECTING,
    MQTT_CONNECTED
};

// Structure globale pour l'état du système
struct SystemStatus {
    WiFiStatus wifi;
    MqttStatus mqtt;
    uint8_t onlineDevices;
    unsigned long lastLoRaRxTime;
    volatile bool mqttNeedsReconnect;
};

// Structure pour les informations d'un module
struct DeviceInfo {
    bool isActive;
    uint8_t nodeId;
    char deviceName[20]; // MAC_XX:XX:XX
    char deviceType[24]; // ex: "WELL_PUMP"
    unsigned long lastSeen;
};

// Structure pour les messages dans la file d'attente LoRa Tx
struct LoRaTxCommand {
    uint8_t targetNodeId;
    char payload[192]; // Le payload est un JSON sérialisé
};
