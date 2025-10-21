#pragma once
#include <Arduino.h>

class LoraNode {
public:
    void init();
    void run(); // Gère la logique de join et l'écoute des commandes
    bool isJoined();
    void sendTelemetry(float temp, float humidity, float voltage, bool pressureOk);

private:
    uint8_t nodeId = 0;
    unsigned long lastTelemetryTime = 0;
    void loadConfig();
    void saveConfig();
    void performJoinRequest();
    void listenForCommands();
};
