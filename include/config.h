#pragma once

// =================================================================
// =================== CONFIGURATION DU PROJET =====================
// =================================================================

#define FIRMWARE_VERSION "1.0.0-prod"

// -------- Configuration WiFi --------
#define WIFI_SSID "VOTRE_SSID_WIFI"
#define WIFI_PASSWORD "VOTRE_MOT_DE_PASSE_WIFI"
#define WIFI_RECONNECT_INTERVAL_MS 30000 // Tentative de reconnexion toutes les 30s

// -------- Configuration MQTT pour ThingsBoard --------
#define TB_SERVER "192.168.1.XX"                 // IP de votre Raspberry Pi
#define TB_PORT 1883
#define TB_GATEWAY_TOKEN "VOTRE_TOKEN_PASSERELLE" // Jeton d'accès de la passerelle
#define MQTT_RECONNECT_INTERVAL_MS 5000         // Tentative de reconnexion toutes les 5s

// -------- Configuration LoRa --------
#define LORA_FREQ 868.0f
#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13
#define LORA_SECRET_KEY "HydrauParkSecret" // Clé secrète de 16 octets pour AES-128
#define LORA_AES_IV "EGCSOHydraulique"   // IV de 16 octets pour AES-128

// -------- Configuration Matérielle (OLED Heltec V3) --------
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define DIAG_BUTTON_PIN 0 // Bouton "PRG" sur la carte Heltec

// -------- Configuration Système --------
#define MAX_DEVICES 20                   // Nombre maximum de modules gérables
#define WATCHDOG_TIMEOUT_S 30            // Timeout du watchdog en secondes
#define DEVICE_OFFLINE_TIMEOUT_MS 300000 // 5 minutes
#define TX_QUEUE_SIZE 10                 // Taille de la file d'attente des commandes LoRa à envoyer
#define RX_QUEUE_SIZE 10                 // Taille de la file d'attente des messages LoRa reçus

// Topics MQTT pour l'API Gateway de ThingsBoard
#define TB_TELEMETRY_TOPIC "v1/gateway/telemetry"
#define TB_CONNECT_TOPIC "v1/gateway/connect"
#define TB_RPC_TOPIC "v1/gateway/rpc"

// Namespace pour le stockage NVS
#define NVS_NAMESPACE "devices"
