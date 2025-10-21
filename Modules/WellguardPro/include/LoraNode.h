#pragma once
#include <Arduino.h>

class LoraNode {
public:
    void init();
    void run();
    bool isJoined();
    void sendTelemetry(float temp, float humidity, float voltage, bool pressureOk);

private:
    uint8_t nodeId = 0;
    uint32_t msgCounter = 0;
    unsigned long lastJoinAttempt = 0;
    unsigned long lastTelemetryTime = 0;

    void loadConfig();
    void saveConfig();
    void performJoinRequest();
    void listenForCommands();
    void sendAck(uint16_t msgId);
    String encryptPayload(const String& plaintext);
    String decryptPayload(const String& b64_ciphertext);
};
