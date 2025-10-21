#pragma once
#include "config.h"
#include "types.h"
#include <ArduinoJson.h>

class DeviceManager {
public:
    DeviceManager();
    void init();
    int8_t registerDevice(const char* mac, const char* type);
    bool isDeviceRegistered(uint8_t nodeId);
    bool isValidMessageCounter(uint8_t nodeId, uint32_t counter);
    void updateDeviceSignalInfo(uint8_t nodeId, float rssi, float snr);
    const char* getDeviceName(uint8_t nodeId);
    uint8_t findNodeIdByName(const char* name);
    uint8_t getOnlineDeviceCount();
    void getAllActiveDeviceNames(JsonArray& deviceList);
    const DeviceInfo* getDeviceInfo(uint8_t index);

private:
    DeviceInfo devices[MAX_DEVICES];
    SemaphoreHandle_t mutex;
    void loadFromNVS();
    void saveToNVS(uint8_t slotIndex);
    uint8_t findEmptySlot();
    int8_t findDeviceByMac(const char* mac);

    void lock();
    void unlock();
};

extern DeviceManager deviceManager;
