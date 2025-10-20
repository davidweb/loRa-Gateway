#pragma once

#include <Arduino.h>

/**
 * @brief Calcule le checksum CRC32 d'un bloc de données.
 *
 * @param data Pointeur vers les données.
 * @param length Longueur des données en octets.
 * @return Le checksum CRC32 calculé.
 */
uint32_t calculateCRC32(const uint8_t *data, size_t length);
