#include "DeviceManager.h"
#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>

DeviceManager deviceManager;
Preferences preferences;

DeviceManager::DeviceManager() {
    mutex = xSemaphoreCreateMutex();
}

void DeviceManager::init() {
    lock();
    for (int i = 0; i < MAX_DEVICES; ++i) {
        devices[i].isActive = false;
        devices[i].nodeId = i + 1; // nodeId de 1 à MAX_DEVICES
    }
    unlock();
    loadFromNVS();
}

void DeviceManager::loadFromNVS() {
    lock();
    preferences.begin(NVS_NAMESPACE, true); // Lecture seule
    JsonDocument doc;
    for (int i = 0; i < MAX_DEVICES; i++) {
        String key = "dev_" + String(i);
        if (preferences.isKey(key.c_str())) {
            String storedDevice = preferences.getString(key.c_str(), "");
            if (deserializeJson(doc, storedDevice) == DeserializationError::Ok) {
                devices[i].isActive = true;
                strncpy(devices[i].deviceName, doc["mac"], sizeof(devices[i].deviceName) - 1);
                strncpy(devices[i].deviceType, doc["type"], sizeof(devices[i].deviceType) - 1);
                Serial.printf("NVS Loaded: Slot %d, MAC: %s, Type: %s\n", i, devices[i].deviceName, devices[i].deviceType);
            }
        }
    }
    preferences.end();
    unlock();
}

void DeviceManager::saveToNVS(uint8_t slotIndex) {
    preferences.begin(NVS_NAMESPACE, false); // Lecture/Écriture
    String key = "dev_" + String(slotIndex);

    JsonDocument doc;
    doc["mac"] = devices[slotIndex].deviceName;
    doc["type"] = devices[slotIndex].deviceType;

    String buffer;
    serializeJson(doc, buffer);

    preferences.putString(key.c_str(), buffer);
    preferences.end();
    Serial.printf("NVS Saved: Slot %d, Data: %s\n", slotIndex, buffer.c_str());
}

int8_t DeviceManager::registerDevice(const char* mac, const char* type) {
    lock();
    int8_t existingId = findDeviceByMac(mac);
    if (existingId != -1) {
        unlock();
        return existingId;
    }
    
    uint8_t slot = findEmptySlot();
    if (slot == 255) {
        unlock();
        return -1; // Plus de place
    }
    
    devices[slot].isActive = true;
    strncpy(devices[slot].deviceName, mac, sizeof(devices[slot].deviceName) - 1);
    strncpy(devices[slot].deviceType, type, sizeof(devices[slot].deviceType) - 1);
    devices[slot].lastSeen = millis();
    uint8_t newId = devices[slot].nodeId;
    
    saveToNVS(slot); // Sauvegarder immédiatement le nouvel appareil
    unlock();
    return newId;
}

bool DeviceManager::isDeviceRegistered(uint8_t nodeId) {
    if (nodeId < 1 || nodeId > MAX_DEVICES) return false;
    lock();
    bool status = devices[nodeId - 1].isActive;
    unlock();
    return status;
}

void DeviceManager::updateDeviceSignalInfo(uint8_t nodeId, float rssi, float snr) {
    if (nodeId < 1 || nodeId > MAX_DEVICES) return;
    lock();
    devices[nodeId - 1].lastSeen = millis();
    devices[nodeId - 1].lastRssi = rssi;
    devices[nodeId - 1].lastSnr = snr;
    unlock();
}

const char* DeviceManager::getDeviceName(uint8_t nodeId) {
    if (!isDeviceRegistered(nodeId)) return "UNKNOWN";
    return devices[nodeId - 1].deviceName;
}

const DeviceInfo* DeviceManager::getDeviceInfo(uint8_t index) {
    if (index >= MAX_DEVICES) return nullptr;
    return &devices[index];
}

uint8_t DeviceManager::findNodeIdByName(const char* name) {
    lock();
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].isActive && strcmp(devices[i].deviceName, name) == 0) {
            uint8_t id = devices[i].nodeId;
            unlock();
            return id;
        }
    }
    unlock();
    return 0; // 0 signifie non trouvé
}

uint8_t DeviceManager::getOnlineDeviceCount() {
    uint8_t count = 0;
    lock();
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].isActive && (millis() - devices[i].lastSeen < DEVICE_OFFLINE_TIMEOUT_MS)) {
            count++;
        }
    }
    unlock();
    return count;
}

void DeviceManager::getAllActiveDeviceNames(JsonArray& deviceList) {
    lock();
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].isActive) {
            deviceList.add(devices[i].deviceName);
        }
    }
    unlock();
}

uint8_t DeviceManager::findEmptySlot() {
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (!devices[i].isActive) return i;
    }
    return 255;
}

int8_t DeviceManager::findDeviceByMac(const char* mac) {
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].isActive && strcmp(devices[i].deviceName, mac) == 0) {
            return devices[i].nodeId;
        }
    }
    return -1;
}

void DeviceManager::lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
void DeviceManager::unlock() { xSemaphoreGive(mutex); }
