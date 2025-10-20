#pragma once
#include <Arduino.h>
void taskLoRaHandler(void *pvParameters);
uint32_t calculateCRC32(const uint8_t *data, size_t length);
