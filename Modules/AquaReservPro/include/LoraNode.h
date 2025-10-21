#pragma once
#include <Arduino.h>

class LoraNode {
public:
    void init();
    void run(); // Gère la logique de join/telemetry
    bool isJoined();
    void sendTelemetry(bool isFull);
    // Note: Pas de gestion des commandes entrantes pour ce module simple

private:
    uint8_t nodeId = 0; // 0 = non joint
    unsigned long lastTelemetryTime = 0;
    void loadConfig();
    void saveConfig();
    void performJoinRequest();
};
