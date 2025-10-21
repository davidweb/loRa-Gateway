#pragma once

#define FIRMWARE_VERSION "1.0.0-WellguardPro"
#define DEVICE_TYPE "WELL_PUMP_STATION"

// -- Configuration Matérielle --
// Pensez à adapter ces pins à votre montage réel
#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13
#define LORA_FREQ 868.0f

#define PUMP_RELAY_PIN 25      // Pin pour le relais de la pompe
#define VOLTAGE_SENSOR_PIN 34  // Pin analogique pour le capteur de tension (ADC1_CH6)
#define PRESSURE_SENSOR_PIN 26 // Pin pour le capteur de pression (contact)
#define DHT_PIN 27             // Pin pour le capteur DHT22
#define DHT_TYPE DHT22

// -- Configuration Système --
#define LORA_SECRET_KEY "HydrauParkSecretKey2025"
#define TELEMETRY_INTERVAL_MS 30000  // Envoi de la télémétrie toutes les 30 secondes
#define SENSOR_READ_INTERVAL_MS 5000 // Lecture des capteurs toutes les 5 secondes

// Namespace pour la sauvegarde en mémoire non-volatile
#define NVS_NAMESPACE "node_config"
