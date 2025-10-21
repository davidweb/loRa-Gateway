#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
#include "config.h"
#include "credentials.h"
#include "LoraNode.h"
#include "helpers.h"
#include "Base64.h"

// ===================== OBJETS GLOBAUX =====================
Module mod(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
SX1262 radio = &mod;
LoraNode loraNode;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Structure pour l'état partagé entre les tâches
struct SharedState {
    bool isFull = false;
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
        .status-indicator { width: 100px; height: 100px; border-radius: 50%; margin: 20px auto; display: flex; justify-content: center; align-items: center; font-size: 18px; font-weight: bold; color: white; transition: background-color: 0.3s; }
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
void connectWiFi();
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void setupWebServer();


void setup() {
    Serial.begin(115200);
    pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);

    stateMutex = xSemaphoreCreateMutex();

    connectWiFi();

    setupWebServer();

    loraNode.init();

    BaseType_t taskSensorsStatus = xTaskCreatePinnedToCore(taskSensors, "Sensors", 2048, NULL, 1, NULL, 0);
    BaseType_t taskLoRaStatus = xTaskCreatePinnedToCore(taskLoRa, "LoRa", 4096, NULL, 1, NULL, 1);

    if (taskSensorsStatus != pdPASS || taskLoRaStatus != pdPASS) {
        Serial.println("Erreur fatale: Impossible de créer les tâches FreeRTOS !");
        ESP.restart();
    }
}

void loop() { vTaskDelete(NULL); }

void connectWiFi() {
    Serial.print("Connexion au WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" Connecté!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        StaticJsonDocument<32> doc;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        doc["isFull"] = sharedState.isFull;
        xSemaphoreGive(stateMutex);
        String output;
        serializeJson(doc, output);
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
        bool rawState = (digitalRead(WATER_LEVEL_PIN) == LOW);

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

            StaticJsonDocument<32> doc;
            doc["isFull"] = lastStableState;
            String output;
            serializeJson(doc, output);
            ws.textAll(output);

            if(loraNode.isJoined()) {
                loraNode.sendTelemetry(lastStableState);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void taskLoRa(void* params) {
    for(;;) {
        loraNode.run();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
