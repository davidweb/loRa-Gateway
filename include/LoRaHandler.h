#pragma once
#include <Arduino.h>

void taskLoRaHandler(void *pvParameters);
uint32_t calculateCRC32(const uint8_t *data, size_t length);
void loraInterrupt();

// Fonctions pour le chiffrement AES
String encrypt_payload(const String& plaintext);
String decrypt_payload(const String& b64_ciphertext);
