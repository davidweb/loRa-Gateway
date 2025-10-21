#pragma once

#define FIRMWARE_VERSION "1.0.0-AquaReservPro"
#define DEVICE_TYPE "RESERVOIR_SENSOR"

// -- Configuration Matérielle --
#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13
#define LORA_FREQ 868.0f

#define WATER_LEVEL_PIN 25 // Pin pour le capteur de contact (flotteur)

// -- Configuration Système --
#define LORA_SECRET_KEY "HydrauParkSecretKey2025"
#define TELEMETRY_INTERVAL_MS 60000 // Envoi de la télémétrie toutes les minutes
#define LEVEL_CONFIRMATION_MS 2000 // Le contact doit être stable pendant 2s pour être confirmé

// Namespace NVS
#define NVS_NAMESPACE "node_config"
