#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiManager.h>
#include <RadioLib.h>
#include <DHT.h>
#include "config.h"
#include "LoraNode.h"

// ===================== OBJETS GLOBAUX =====================
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
LoraNode loraNode;
DHT dht(DHT_PIN, DHT_TYPE);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Structure pour l'état partagé entre les tâches, protégée par un sémaphore
struct SharedState {
    bool pumpOn = false;
    bool pressureOk = false;
    float temperature = 0.0;
    float humidity = 0.0;
    float voltage = 0.0;
};
SharedState sharedState;
SemaphoreHandle_t stateMutex;

// Contenu HTML/JS complet de l'interface web
const char* HTML_CONTENT = R"rawliteral(
<!DOCTYPE html>
<html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>WellguardPro</title>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh;background:#f0f2f5;margin:0}
.container{background:white;padding:2em;border-radius:12px;box-shadow:0 6px 20px rgba(0,0,0,0.1);width:90%;max-width:500px}
h1{text-align:center;color:#1c1e21;margin-bottom:1.5em}
.control-panel,.sensor-grid{display:grid;gap:1.5em;margin-top:1.5em}
.sensor-grid{grid-template-columns:repeat(auto-fit,minmax(130px,1fr))}
.card{background:#f8f9fa;padding:1.2em;border-radius:8px;text-align:center;border:1px solid #dee2e6}
.card h3{margin:0 0 0.5em;color:#495057;font-size:1em;font-weight:600}
.card .value{font-size:1.8em;font-weight:bold;color:#212529}
.switch{position:relative;display:inline-block;width:80px;height:40px}.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#dc3545;transition:.4s;border-radius:40px}
.slider:before{position:absolute;content:"";height:32px;width:32px;left:4px;bottom:4px;background-color:white;transition:.4s;border-radius:50%}
input:checked+.slider{background-color:#28a745}
input:checked+.slider:before{transform:translateX(40px)}
</style></head><body>
<div class="container"><h1>WellguardPro Control</h1>
<div class="control-panel"><div class="card"><h3>Pompe</h3><label class="switch"><input type="checkbox" id="pump-switch"><span class="slider"></span></label></div></div>
<div class="sensor-grid">
<div class="card"><h3>Température</h3><span id="temp-val" class="value">--</span> °C</div>
<div class="card"><h3>Humidité</h3><span id="hum-val" class="value">--</span> %</div>
<div class="card"><h3>Tension</h3><span id="volt-val" class="value">--</span> V</div>
<div class="card"><h3>Pression</h3><span id="pres-val" class="value" style="font-size:1.5em;">--</span></div>
</div></div>
<script>
const ws=new WebSocket(`ws://${window.location.host}/ws`);
const pumpSwitch=document.getElementById('pump-switch');
function updateUI(data){
    if(data.hasOwnProperty('pumpOn')) pumpSwitch.checked=data.pumpOn;
    if(data.hasOwnProperty('temperature')) document.getElementById('temp-val').innerText=data.temperature.toFixed(1);
    if(data.hasOwnProperty('humidity')) document.getElementById('hum-val').innerText=data.humidity.toFixed(1);
    if(data.hasOwnProperty('voltage')) document.getElementById('volt-val').innerText=data.voltage.toFixed(2);
    if(data.hasOwnProperty('pressureOk')) document.getElementById('pres-val').innerText=data.pressureOk?'OK':'Faible';
}
ws.onmessage=event=>updateUI(JSON.parse(event.data));
pumpSwitch.addEventListener('change',()=>{ws.send(JSON.stringify({action:'setPump',state:pumpSwitch.checked}));});
</script></body></html>
)rawliteral";

// ===================== PROTOTYPES =====================
void taskLoRa(void* params);
void taskSensors(void* params);
void setupWebServer();
void setPumpState(bool state, bool fromLora);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

void setup() {
    Serial.begin(115200);
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    digitalWrite(PUMP_RELAY_PIN, LOW); // Sécurité : Pompe éteinte au démarrage
    pinMode(PRESSURE_SENSOR_PIN, INPUT_PULLUP);
    dht.begin();
    
    stateMutex = xSemaphoreCreateMutex();

    WiFiManager wm;
    wm.autoConnect("WellguardPro-Setup");
    Serial.println("WiFi connecté !");

    setupWebServer();
    loraNode.init();

    xTaskCreatePinnedToCore(taskSensors, "Sensors", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskLoRa, "LoRa", 4096, NULL, 1, NULL, 1);
}

void loop() { vTaskDelete(NULL); }

// Fonction centrale et sécurisée pour changer l'état de la pompe
void setPumpState(bool state, bool fromLora = false) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    sharedState.pumpOn = state;
    digitalWrite(PUMP_RELAY_PIN, state ? HIGH : LOW);
    SharedState stateCopy = sharedState; // Copie pour l'envoi de notifications
    xSemaphoreGive(stateMutex);
    
    Serial.printf("Pompe mise à %s (source: %s)\n", state ? "ON" : "OFF", fromLora ? "LoRa" : "Web");
    
    // Notifier immédiatement les clients web de ce changement
    StaticJsonDocument<128> doc;
    doc["pumpOn"] = stateCopy.pumpOn; // On ne notifie que l'état de la pompe
    String output; serializeJson(doc, output);
    ws.textAll(output);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        // Envoi de l'état complet au nouveau client
        StaticJsonDocument<128> doc;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        doc["pumpOn"] = sharedState.pumpOn;
        doc["temperature"] = sharedState.temperature;
        doc["humidity"] = sharedState.humidity;
        doc["voltage"] = sharedState.voltage;
        doc["pressureOk"] = sharedState.pressureOk;
        xSemaphoreGive(stateMutex);
        String output; serializeJson(doc, output);
        client->text(output);
    } else if (type == WS_EVT_DATA) {
        // Traitement de la commande venant du client web
        StaticJsonDocument<64> doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
            if (strcmp(doc["action"], "setPump") == 0) {
                setPumpState(doc["state"]);
            }
        }
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
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));

        SharedState stateCopy;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        // Lire les capteurs et mettre à jour l'état partagé
        sharedState.humidity = dht.readHumidity();
        sharedState.temperature = dht.readTemperature();
        sharedState.pressureOk = (digitalRead(PRESSURE_SENSOR_PIN) == HIGH);
        sharedState.voltage = analogRead(VOLTAGE_SENSOR_PIN) * (3.3 / 4095.0) * 11.0; // Exemple avec diviseur de tension 10:1
        stateCopy = sharedState;
        xSemaphoreGive(stateMutex);
        
        // Notifier les clients web de la mise à jour des capteurs
        StaticJsonDocument<128> doc;
        doc["temperature"] = stateCopy.temperature;
        doc["humidity"] = stateCopy.humidity;
        doc["voltage"] = stateCopy.voltage;
        doc["pressureOk"] = stateCopy.pressureOk;
        String output; serializeJson(doc, output);
        ws.textAll(output);

        // Envoyer la télémétrie LoRa (la fonction gère elle-même l'intervalle de temps)
        loraNode.sendTelemetry(stateCopy.temperature, stateCopy.humidity, stateCopy.voltage, stateCopy.pressureOk);
    }
}

void taskLoRa(void* params) {
    for(;;) {
        loraNode.run();
        vTaskDelay(pdMS_TO_TICKS(100)); // Courte pause pour laisser du temps aux autres tâches
    }
}
