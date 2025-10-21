#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiManager.h>
#include <RadioLib.h>
#include "config.h"
#include "LoraNode.h"

// ===================== OBJETS GLOBAUX =====================
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
LoraNode loraNode;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Structure pour l'état partagé entre les tâches
struct SharedState {
    bool isFull = false;
    bool isWifiConnected = false;
};
SharedState sharedState;
SemaphoreHandle_t stateMutex;

// Contenu de la page web
const char* HTML_CONTENT = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AquaReservPro</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f2f5; margin: 0; }
        .container { background-color: white; padding: 40px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); text-align: center; width: 90%; max-width: 400px; }
        h1 { color: #1c1e21; margin-bottom: 20px; }
        .status-indicator { width: 100px; height: 100px; border-radius: 50%; margin: 20px auto; display: flex; justify-content: center; align-items: center; font-size: 18px; font-weight: bold; color: white; transition: background-color 0.3s; }
        .status-full { background-color: #28a745; }
        .status-empty { background-color: #dc3545; }
        .info { font-size: 14px; color: #606770; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>AquaReservPro</h1>
        <div id="level-indicator" class="status-indicator status-empty">Vide</div>
        <p class="info">Version: 1.0.0-AquaReservPro</p>
    </div>
    <script>
        const ws = new WebSocket(`ws://${window.location.host}/ws`);
        const levelIndicator = document.getElementById('level-indicator');
        
        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            if (data.isFull) {
                levelIndicator.className = 'status-indicator status-full';
                levelIndicator.innerText = 'Plein';
            } else {
                levelIndicator.className = 'status-indicator status-empty';
                levelIndicator.innerText = 'Vide';
            }
        };
    </script>
</body>
</html>
)rawliteral";

// ===================== PROTOTYPES DES TÂCHES =====================
void taskLoRa(void* params);
void taskSensors(void* params);
void setupWebServer();

void setup() {
    Serial.begin(115200);
    pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);

    stateMutex = xSemaphoreCreateMutex();

    // Connexion WiFi
    WiFiManager wm;
    wm.autoConnect("AquaReservPro-Setup");
    Serial.println("WiFi connecté !");
    sharedState.isWifiConnected = true;

    // Initialisation du serveur web
    setupWebServer();

    // Initialisation LoRa
    loraNode.init();

    // Création des tâches
    xTaskCreatePinnedToCore(taskSensors, "Sensors", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskLoRa, "LoRa", 4096, NULL, 1, NULL, 1);
}

void loop() { vTaskDelete(NULL); }

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        // Envoi de l'état actuel au nouveau client
        StaticJsonDocument<32> doc;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        doc["isFull"] = sharedState.isFull;
        xSemaphoreGive(stateMutex);
        String output; serializeJson(doc, output);
        client->text(output);
    }
}

void setupWebServer() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", HTML_CONTENT);
    });
    server.begin();
}

void taskSensors(void* params) {
    unsigned long lastChangeTime = 0;
    bool lastStableState = false;
    bool currentState = false;

    for(;;) {
        // Logique de confirmation temporelle (debounce)
        bool rawState = (digitalRead(WATER_LEVEL_PIN) == LOW); // LOW = contact = plein

        if (rawState != currentState) {
            currentState = rawState;
            lastChangeTime = millis();
        }

        if ((millis() - lastChangeTime > LEVEL_CONFIRMATION_MS) && (currentState != lastStableState)) {
            lastStableState = currentState;
            Serial.printf("Nouvel état de niveau confirmé : %s\n", lastStableState ? "PLEIN" : "VIDE");
            
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            sharedState.isFull = lastStableState;
            xSemaphoreGive(stateMutex);
            
            // Notifier les clients WebSocket
            StaticJsonDocument<32> doc;
            doc["isFull"] = lastStableState;
            String output; serializeJson(doc, output);
            ws.textAll(output);

            // Déclencher l'envoi de la télémétrie LoRa
            if(loraNode.isJoined()) {
                loraNode.sendTelemetry(lastStableState);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Vérifier le capteur 10 fois par seconde
    }
}

void taskLoRa(void* params) {
    for(;;) {
        loraNode.run(); // Gère la logique de join
        vTaskDelay(pdMS_TO_TICKS(10000)); // Pause de 10s entre les tentatives de join si nécessaire
    }
}
