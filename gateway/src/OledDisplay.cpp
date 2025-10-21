#include "OledTask.h"
#include "config.h"
#include "types.h"
#include "DeviceManager.h"
#include <Heltec.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

// Utilise Heltec.display directement
extern SystemStatus systemStatus;

// États de l'affichage
enum DisplayPage {
    PAGE_HOME,
    PAGE_DEVICES,
    PAGE_SYSTEM_STATS
};
DisplayPage currentPage = PAGE_HOME;

void taskOledDisplay(void *pvParameters) {
    esp_task_wdt_add(NULL);
    Serial.println("OLED Task started");

    pinMode(DIAG_BUTTON_PIN, INPUT_PULLUP);
    long lastButtonPress = 0;

    for (;;) {
        esp_task_wdt_reset();

        // Gestion du changement de page
        if (digitalRead(DIAG_BUTTON_PIN) == LOW && (millis() - lastButtonPress > 500)) {
            lastButtonPress = millis();
            currentPage = (DisplayPage)(((int)currentPage + 1) % 3);
        }
        
        systemStatus.onlineDevices = deviceManager.getOnlineDeviceCount();
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display->setFont(ArialMT_Plain_10);
        char buffer[64];

        switch (currentPage) {
            case PAGE_HOME: {
                Heltec.display->drawString(0, 0, "==== ECRAN PRINCIPAL ====");
                switch(systemStatus.wifi) {
                    case WIFI_CONNECTED: snprintf(buffer, sizeof(buffer), "IP: %s", WiFi.localIP().toString().c_str()); break;
                    case WIFI_CONNECTING: strncpy(buffer, "WiFi: Connexion...", sizeof(buffer)); break;
                    default: strncpy(buffer, "WiFi: Deconnecte", sizeof(buffer)); break;
                }
                Heltec.display->drawString(0, 12, buffer);

                switch(systemStatus.mqtt) {
                    case GW_MQTT_CONNECTED: strncpy(buffer, "ThingsBoard: Connecte", sizeof(buffer)); break;
                    case GW_MQTT_CONNECTING: strncpy(buffer, "ThingsBoard: Connexion...", sizeof(buffer)); break;
                    default: strncpy(buffer, "ThingsBoard: Deconnecte", sizeof(buffer)); break;
                }
                Heltec.display->drawString(0, 24, buffer);

                snprintf(buffer, sizeof(buffer), "Modules en Ligne: %d", systemStatus.onlineDevices);
                Heltec.display->drawString(0, 36, buffer);

                if (systemStatus.lastLoRaRxTime > 0) {
                    snprintf(buffer, sizeof(buffer), "LoRa RX: il y a %lus", (millis() - systemStatus.lastLoRaRxTime) / 1000);
                } else {
                    strncpy(buffer, "LoRa RX: N/A", sizeof(buffer));
                }
                Heltec.display->drawString(0, 48, buffer);
                break;
            }
            case PAGE_DEVICES: {
                Heltec.display->drawString(0, 0, "==== MODULES LORA ====");
                int y = 12;
                for (int i = 0; i < MAX_DEVICES; i++) {
                    const DeviceInfo* device = deviceManager.getDeviceInfo(i);
                    if (device && device->isActive) {
                        bool isOnline = (millis() - device->lastSeen) < DEVICE_OFFLINE_TIMEOUT_MS;
                        snprintf(buffer, sizeof(buffer), "%s #%d: R:%.1f S:%.1f %s",
                            device->deviceType,
                            device->nodeId,
                            device->lastRssi,
                            device->lastSnr,
                            isOnline ? "ON" : "OFF"
                        );
                        Heltec.display->drawString(0, y, buffer);
                        y += 10;
                        if (y > 54) break;
                    }
                }
                break;
            }
            case PAGE_SYSTEM_STATS: {
                Heltec.display->drawString(0, 0, "==== STATS SYSTEME ====");
                snprintf(buffer, sizeof(buffer), "Heap Libre: %u", esp_get_free_heap_size());
                Heltec.display->drawString(0, 12, buffer);
                snprintf(buffer, sizeof(buffer), "Uptime: %lum", millis() / 60000);
                Heltec.display->drawString(0, 24, buffer);
                snprintf(buffer, sizeof(buffer), "FW: %s", FIRMWARE_VERSION);
                Heltec.display->drawString(0, 36, buffer);
                break;
            }
        }
    Heltec.display->display();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
