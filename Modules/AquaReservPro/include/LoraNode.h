#pragma once
#include <Arduino.h>

class LoraNode {
public:
    void init();
    void run();
    bool isJoined();
    void sendTelemetry(bool isFull);

private:
    uint8_t nodeId = 0;
    uint32_t msgCounter = 0; // Compteur de messages pour la sécurité
    unsigned long lastJoinAttempt = 0;

    void loadConfig();
    void saveConfig();
    void performJoinRequest();
    String encryptPayload(const String& plaintext);
    String decryptPayload(const String& b64_ciphertext);
};
