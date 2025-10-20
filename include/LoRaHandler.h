#pragma once
#include <Arduino.h>

void taskLoRaHandler(void *pvParameters);
void loraInterrupt();

// Fonctions pour le chiffrement AES
String encrypt_payload(const String& plaintext);
String decrypt_payload(const String& b64_ciphertext);
