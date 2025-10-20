#pragma once
#include "types.h"
#include <ArduinoJson.h>

class DeviceManager {
public:
    DeviceManager();
    void init();
    int8_t registerDevice(const char* mac, const char* type);
    bool isDeviceRegistered(uint8_t nodeId);
    void updateDeviceLastSeen(uint8_t nodeId);
    const char* getDeviceName(uint8_t nodeId);
    uint8_t findNodeIdByName(const char* name);
    uint8_t getOnlineDeviceCount();
    void getAllActiveDeviceNames(JsonArray& deviceList);

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
