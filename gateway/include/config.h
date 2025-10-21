#pragma once

// =================================================================
// =================== CONFIGURATION DU PROJET =====================
// =================================================================

#define FIRMWARE_VERSION "1.0.1-alpha"

// -------- Fichier des identifiants --------
// Les informations sensibles sont stockées dans `credentials.h`, qui n'est pas suivi par Git.
#if __has_include("credentials.h")
#include "credentials.h"
#else
#error "Le fichier 'include/credentials.h' est manquant. Veuillez le créer à partir de 'credentials.h.example' et y renseigner vos informations."
#endif

// -------- Configuration WiFi --------
#define WIFI_RECONNECT_INTERVAL_MS 30000 // Tentative de reconnexion toutes les 30s

// -------- Configuration MQTT pour ThingsBoard --------
#define TB_PORT 1883
#define MQTT_RECONNECT_INTERVAL_MS 5000         // Tentative de reconnexion toutes les 5s

// -------- Configuration LoRa --------
#define LORA_FREQ 868.0f
#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13

// -------- Configuration Matérielle (OLED Heltec V3) --------
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
