#include <Arduino.h>
#include <WiFi.h>
#include <RadioLib.h>
#include <SSD1306Wire.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "types.h"
#include "DeviceManager.h"
#include "OledDisplay.h"
#include "MqttHandler.h"
#include "LoRaHandler.h"

extern void loraInterrupt();

// ===================== OBJETS GLOBAUX =====================
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

SystemStatus systemStatus = { WIFI_DISCONNECTED, GW_MQTT_DISCONNECTED, 0, 0 };

// ===================== OBJETS FreeRTOS =====================
QueueHandle_t loraTxQueue;
QueueHandle_t loraRxQueue;
QueueHandle_t systemQueue;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=============================================");
    Serial.println("Démarrage Passerelle Hydraulique EGCSO");
    Serial.printf("Version: %s\n", FIRMWARE_VERSION);
    Serial.println("=============================================");

    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL); // Le setup est aussi surveillé

    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, HIGH); delay(10); digitalWrite(OLED_RST, LOW); delay(10); digitalWrite(OLED_RST, HIGH);
    if (!display.init()) {
        Serial.println("Erreur: Impossible d'initialiser l'écran OLED.");
    }
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.clear();
    display.drawString(0, 10, "Boot Passerelle...");
    display.display();

    int state = radio.begin(LORA_FREQ);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("Init LoRa echec, code: %d. Redemarrage...\n", state);
        display.drawString(0, 30, "Erreur LoRa!");
        display.display();
        delay(5000);
        ESP.restart();
    }
    radio.setDio1Action(loraInterrupt);
    Serial.println("Module LoRa (SX1262) OK.");

    deviceManager.init();
    Serial.println("Device Manager initialisé.");

    loraTxQueue = xQueueCreate(TX_QUEUE_SIZE, sizeof(LoRaTxCommand));
    loraRxQueue = xQueueCreate(RX_QUEUE_SIZE, sizeof(StaticJsonDocument<256>));
    systemQueue = xQueueCreate(5, sizeof(SystemEvent));
    if (!loraTxQueue || !loraRxQueue || !systemQueue) {
        Serial.println("Erreur: Impossible de créer les files d'attente. Redemarrage...");
        delay(5000);
        ESP.restart();
    }

    BaseType_t oledTaskStatus = xTaskCreatePinnedToCore(taskOledDisplay, "OLED", 3072, NULL, 1, NULL, 0);
    BaseType_t mqttTaskStatus = xTaskCreatePinnedToCore(taskMqttHandler, "MQTT", 4096, NULL, 2, NULL, 0);
    BaseType_t loraTaskStatus = xTaskCreatePinnedToCore(taskLoRaHandler, "LoRa", 4096, NULL, 2, NULL, 1);

    if (oledTaskStatus != pdPASS || mqttTaskStatus != pdPASS || loraTaskStatus != pdPASS) {
        Serial.println("Erreur: Impossible de créer une ou plusieurs tâches FreeRTOS. Redemarrage...");
        delay(5000);
        ESP.restart();
    }
    
    esp_task_wdt_delete(NULL); // Fin de la surveillance du setup
    Serial.println("Tâches FreeRTOS démarrées. Le système est opérationnel.");
}

void loop() {
    vTaskDelete(NULL);
}
