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
    GW_MQTT_DISCONNECTED,
    GW_MQTT_CONNECTING,
    GW_MQTT_CONNECTED
};

// Énumération pour les événements système
enum SystemEventType {
    NEW_DEVICE_REGISTERED
};

// Structure pour les messages d'événements système
struct SystemEvent {
    SystemEventType type;
    uint8_t nodeId;
};

// Structure globale pour l'état du système
struct SystemStatus {
    WiFiStatus wifi;
    MqttStatus mqtt;
    uint8_t onlineDevices;
    unsigned long lastLoRaRxTime;
};

// Structure pour les informations d'un module
struct DeviceInfo {
    bool isActive;
    uint8_t nodeId;
    char deviceName[20]; // MAC_XX:XX:XX
    char deviceType[24]; // ex: "WELL_PUMP"
    unsigned long lastSeen;
    float lastRssi;
    float lastSnr;
};

// Structure pour les messages dans la file d'attente LoRa Tx
struct LoRaTxCommand {
    uint8_t targetNodeId;
    char payload[192]; // Le payload est un JSON sérialisé
    uint16_t msgId;
    bool requireAck;
};

// =================================================================
// =================== CONSTANTES DU PROTOCOLE =====================
// =================================================================

// Types de messages LoRa
constexpr const char* LORA_MSG_TYPE_JOIN_REQUEST = "JOIN_REQUEST";
constexpr const char* LORA_MSG_TYPE_JOIN_ACCEPT = "JOIN_ACCEPT";
constexpr const char* LORA_MSG_TYPE_TELEMETRY = "TELEMETRY";
constexpr const char* LORA_MSG_TYPE_CMD = "CMD";
constexpr const char* LORA_MSG_TYPE_ACK = "ACK";

// Clés JSON du protocole LoRa
constexpr const char* LORA_KEY_MSG_ID = "msgId";
constexpr const char* LORA_KEY_PAYLOAD = "p";
constexpr const char* LORA_KEY_CRC = "c";
constexpr const char* LORA_KEY_TYPE = "type";
constexpr const char* LORA_KEY_MAC = "mac";
constexpr const char* LORA_KEY_SECRET = "secret";
constexpr const char* LORA_KEY_DEV_TYPE = "devType";
constexpr const char* LORA_KEY_NODE_ID = "nodeId";
constexpr const char* LORA_KEY_DATA = "data";
constexpr const char* LORA_KEY_METHOD = "method";
constexpr const char* LORA_KEY_PARAMS = "params";

// Méthodes RPC reconnues
constexpr const char* LORA_METHOD_SET_CONFIG = "set_config";
