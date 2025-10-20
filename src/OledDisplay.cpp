#include "OledDisplay.h"
#include "config.h"
#include "types.h"
#include "DeviceManager.h"
#include <SSD1306Wire.h>
#include <WiFi.h>

extern SSD1306Wire display;
extern SystemStatus systemStatus;

void taskOledDisplay(void *pvParameters) {
    esp_task_wdt_add(NULL);
    Serial.println("OLED Task started");
    char buffer[40];

    for (;;) {
        esp_task_wdt_reset();
        
        systemStatus.onlineDevices = deviceManager.getOnlineDeviceCount();

        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_10);
        
        switch(systemStatus.wifi) {
            case WIFI_CONNECTED: snprintf(buffer, sizeof(buffer), "IP: %s", WiFi.localIP().toString().c_str()); break;
            case WIFI_CONNECTING: strcpy(buffer, "WiFi: Connexion..."); break;
            default: strcpy(buffer, "WiFi: Deconnecte"); break;
        }
        display.drawString(0, 0, buffer);
        
        switch(systemStatus.mqtt) {
            case MQTT_CONNECTED: strcpy(buffer, "ThingsBoard: Connecte"); break;
            case MQTT_CONNECTING: strcpy(buffer, "ThingsBoard: Connexion..."); break;
            default: strcpy(buffer, "ThingsBoard: Deconnecte"); break;
        }
        display.drawString(0, 12, buffer);

        display.drawHorizontalLine(0, 24, 128);

        snprintf(buffer, sizeof(buffer), "Modules en Ligne: %d", systemStatus.onlineDevices);
        display.drawString(0, 28, buffer);
        
        if (systemStatus.lastLoRaRxTime > 0) {
            snprintf(buffer, sizeof(buffer), "LoRa RX: il y a %lus", (millis() - systemStatus.lastLoRaRxTime) / 1000);
        } else {
            strcpy(buffer, "LoRa RX: N/A");
        }
        display.drawString(0, 40, buffer);

        snprintf(buffer, sizeof(buffer), "FW: %s", FIRMWARE_VERSION);
        display.drawString(0, 52, buffer);

        display.display();
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
