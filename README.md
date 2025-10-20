# Passerelle LoRa "Plug and Play" pour ThingsBoard

Ce projet implémente une passerelle LoRa robuste et "plug and play" conçue pour fonctionner sur un module Heltec WiFi LoRa V3. Elle sert de pont entre un réseau de capteurs/actionneurs LoRa et une instance ThingsBoard, idéale pour la supervision de systèmes industriels comme un parc hydraulique.

L'architecture est basée sur **FreeRTOS** pour une gestion temps réel fiable des communications, et le code est structuré de manière modulaire pour une maintenance et une évolutivité aisées.

 <!-- Pensez à remplacer ce lien par une vraie capture d'écran de votre OLED en fonctionnement -->

## Table des Matières
- [Fonctionnalités Clés](#fonctionnalités-clés)
- [Matériel Requis](#matériel-requis)
- [Installation et Déploiement](#installation-et-déploiement)
  - [1. Prérequis](#1-prérequis)
  - [2. Clonage du Dépôt](#2-clonage-du-dépôt)
  - [3. Configuration](#3-configuration)
  - [4. Compilation et Téléversement](#4-compilation-et-téléversement)
- [Architecture Logicielle](#architecture-logicielle)
- [Protocoles de Communication](#protocoles-de-communication)
  - [Protocole LoRa (Module <-> Passerelle)](#protocole-lora-module---passerelle)
  - [Protocole MQTT (Passerelle <-> ThingsBoard)](#protocole-mqtt-passerelle---thingsboard)
- [Fonctionnement du "Plug and Play"](#fonctionnement-du-plug-and-play)
- [Exemple de Code pour un Module Capteur](#exemple-de-code-pour-un-module-capteur)
- [Contribution](#contribution)
- [Licence](#licence)

## Fonctionnalités Clés

*   **Plug and Play Automatisé** : Les nouveaux modules (capteurs/actionneurs) peuvent rejoindre le réseau de manière sécurisée. Une fois authentifiés, ils sont automatiquement enregistrés et leurs données sont relayées à ThingsBoard.
*   **Persistance des Données** : La liste des modules autorisés est sauvegardée en mémoire non-volatile (NVS). En cas de redémarrage, la passerelle se souvient de tous les appareils, qui peuvent se reconnecter instantanément.
*   **Haute Stabilité (FreeRTOS)** : Toutes les opérations critiques (LoRa, WiFi/MQTT, Affichage) tournent dans des tâches FreeRTOS dédiées et sont supervisées par un watchdog, garantissant une haute disponibilité.
*   **Intégrité des Messages** : Chaque message LoRa est validé par un **CRC32** pour garantir que seules les données non corrompues sont traitées.
*   **Supervision Locale** : Un écran OLED intégré fournit des informations vitales en temps réel sur l'état du WiFi, la connexion à ThingsBoard, le nombre de modules en ligne et l'activité LoRa.
*   **Architecture Modulaire** : Le code est séparé en modules logiques (Gestionnaire de modules, Handler LoRa, Handler MQTT, etc.) pour une lisibilité et une maintenance simplifiées.

## Matériel Requis

1.  **Passerelle** : 1x [Heltec WiFi LoRa 32 (V3)](https://heltec.org/project/wifi-lora-32-v3/).
2.  **Modules Distants** : Plusieurs modules ESP32 équipés d'une puce LoRa (ex: Heltec WiFi LoRa 32 V3, TTGO LoRa32, etc.).
3.  **Serveur ThingsBoard** : Une instance de ThingsBoard Community Edition, par exemple sur un Raspberry Pi ou un serveur dédié.

## Installation et Déploiement

### 1. Prérequis

*   [Visual Studio Code](https://code.visualstudio.com/) avec l'extension [PlatformIO IDE](https://platformio.org/install/ide?install=vscode).
*   Avoir configuré une **Gateway** dans votre instance ThingsBoard et récupéré son **jeton d'accès (Access Token)**.

### 2. Clonage du Dépôt

```bash
git clone https://github.com/VOTRE_NOM_UTILISATEUR/VOTRE_NOM_DE_DEPOT.git
cd VOTRE_NOM_DE_DEPOT
```

Ouvrez le dossier du projet dans Visual Studio Code. PlatformIO détectera automatiquement le fichier `platformio.ini` et installera les bibliothèques requises.

### 3. Configuration

Le fichier le plus important à modifier est `/include/config.h`. Remplissez les champs suivants avec vos propres informations :

```cpp
// -------- Configuration WiFi --------
#define WIFI_SSID "VOTRE_SSID_WIFI"
#define WIFI_PASSWORD "VOTRE_MOT_DE_PASSE_WIFI"

// -------- Configuration MQTT pour ThingsBoard --------
#define TB_SERVER "192.168.1.XX"                 // IP de votre serveur ThingsBoard
#define TB_GATEWAY_TOKEN "VOTRE_TOKEN_PASSERELLE" // Jeton d'accès de la passerelle

// -------- Configuration LoRa --------
#define LORA_SECRET_KEY "HydrauParkSecretKey2025" // Changez cette clé pour votre déploiement !
```

### 4. Compilation et Téléversement

Une fois la configuration terminée, utilisez les commandes de PlatformIO pour compiler et téléverser le firmware sur votre Heltec V3 :
*   Cliquez sur l'icône **PlatformIO (tête de fourmi)** dans la barre de gauche.
*   Sous le menu `heltec_wifi_lora_32_v3`, cliquez sur **Build** pour compiler, puis **Upload** pour téléverser.
*   Ouvrez le **Serial Monitor** pour suivre le processus de démarrage de la passerelle.

## Architecture Logicielle

Le projet est divisé en plusieurs modules pour une séparation claire des responsabilités :
*   `main.cpp`: Point d'entrée du programme. Initialise les modules et lance les tâches FreeRTOS.
*   `DeviceManager`: Le cœur de la logique "Plug and Play". Gère l'enregistrement, l'authentification et l'état des modules. Assure la persistance des données en NVS.
*   `LoRaHandler`: Gère exclusivement la communication radio LoRa (émission, réception, validation CRC32).
*   `MqttHandler`: Gère la connexion au broker MQTT de ThingsBoard, la publication de la télémétrie et la souscription aux commandes RPC.
*   `OledDisplay`: Gère l'affichage des informations de statut sur l'écran OLED.
*   `include/types.h`: Définit les structures de données et énumérations globales utilisées à travers le projet.
*   `include/config.h`: Fichier de configuration centralisé.

## Protocoles de Communication

### Protocole LoRa (Module <-> Passerelle)

Tous les messages LoRa sont des chaînes JSON encapsulées dans une structure qui inclut un payload (`p`) et un checksum (`c`).

**Structure Générale :**
```json
{
  "p": { ... payload JSON ... },
  "c": 1234567890 
}
```
Où `c` est le CRC32 du payload `p` sérialisé en chaîne de caractères.

**1. Demande d'Adhésion (`JOIN_REQUEST`)** (Module -> Passerelle)
Envoyé par un nouveau module pour s'enregistrer.
```json
{
  "p": {
    "type": "JOIN_REQUEST",
    "mac": "AA:BB:CC:11:22:33",
    "secret": "HydrauParkSecretKey2025",
    "devType": "WELL_PUMP_STATION"
  },
  "c": 3130043812
}
```

**2. Acceptation d'Adhésion (`JOIN_ACCEPT`)** (Passerelle -> Module)
Réponse de la passerelle avec un identifiant réseau court.
```json
{
  "p": {
    "type": "JOIN_ACCEPT",
    "nodeId": 5
  },
  "c": 1399411918
}
```

**3. Télémétrie (`TELEMETRY`)** (Module -> Passerelle)
Message standard pour envoyer des données de capteurs.
```json
{
  "p": {
    "type": "TELEMETRY",
    "nodeId": 5,
    "data": {
      "temperature": 25.4,
      "pressure": 1.2,
      "voltage": 231.5
    }
  },
  "c": 2831513745
}
```

**4. Commande (`CMD`)** (Passerelle -> Module)
Envoyé par la passerelle pour piloter un actionneur, suite à une commande RPC de ThingsBoard.
```json
{
  "p": {
    "type": "CMD",
    "method": "setRelay",
    "params": {
      "relayIndex": 1,
      "state": true
    }
  },
  "c": 4147814030
}```

### Protocole MQTT (Passerelle <-> ThingsBoard)

La communication se fait via l'**API Gateway** de ThingsBoard.

*   **Connexion d'un module** : `v1/gateway/connect`
    ```json
    {"device": "AA:BB:CC:11:22:33"}
    ```
*   **Envoi de télémétrie** : `v1/gateway/telemetry`
    ```json
    {
      "AA:BB:CC:11:22:33": [
        {
          "ts": 1678886400000,
          "values": {
            "temperature": 25.4,
            "pressure": 1.2
          }
        }
      ]
    }
    ```
*   **Réception de commandes** : `v1/gateway/rpc`
    La passerelle s'abonne à ce topic pour recevoir les commandes RPC destinées à ses modules.

## Fonctionnement du "Plug and Play"

1.  Un module est mis sous tension pour la première fois. Il ne possède pas de `nodeId` stocké en mémoire.
2.  Il envoie un message `JOIN_REQUEST` via LoRa, contenant son adresse MAC, le secret partagé et son type.
3.  La passerelle reçoit la requête. Le `LoRaHandler` valide le CRC32.
4.  Le `DeviceManager` vérifie la validité du secret.
5.  Si tout est correct, le `DeviceManager` enregistre l'adresse MAC et le type du module dans la mémoire NVS et lui assigne le premier `nodeId` disponible.
6.  La passerelle répond avec un message `JOIN_ACCEPT` contenant le `nodeId` attribué.
7.  Le module reçoit la réponse, sauvegarde son `nodeId` dans sa propre NVS et peut commencer à envoyer des messages de `TELEMETRY`.
8.  Au prochain redémarrage, le module lira son `nodeId` en NVS et commencera directement à envoyer de la télémétrie, sans refaire de `JOIN_REQUEST`.

## Exemple de Code pour un Module Capteur

Voici une ébauche de code pour un module distant, illustrant la logique de communication :
```cpp
// Ceci est un exemple conceptuel pour un module capteur/actionneur.
#include <Arduino.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ... Définitions des pins LoRa, secret, etc. ...
#define LORA_SECRET_KEY "HydrauParkSecretKey2025"

SX1262 radio = new Module(...);
Preferences preferences;
uint8_t myNodeId = 0; // 0 = non enregistré

// Fonction pour calculer le CRC32 (doit être la même que sur la passerelle)
uint32_t calculateCRC32(const uint8_t *data, size_t length);

void sendLoRaMessage(JsonDocument& doc) {
    String payloadStr;
    serializeJson(doc["p"], payloadStr);
    doc["c"] = calculateCRC32((const uint8_t*)payloadStr.c_str(), payloadStr.length());
    
    String msg;
    serializeJson(doc, msg);
    radio.transmit(msg);
}

void attemptJoin() {
    StaticJsonDocument<256> doc;
    JsonObject p = doc.createNestedObject("p");
    p["type"] = "JOIN_REQUEST";
    p["mac"] = WiFi.macAddress();
    p["secret"] = LORA_SECRET_KEY;
    p["devType"] = "WATER_LEVEL_SENSOR";
    
    sendLoRaMessage(doc);

    // Mettre en place une logique d'écoute pour la réponse JOIN_ACCEPT
    // et stocker le nodeId reçu en NVS.
}

void sendTelemetry() {
    StaticJsonDocument<256> doc;
    JsonObject p = doc.createNestedObject("p");
    p["type"] = "TELEMETRY";
    p["nodeId"] = myNodeId;
    
    JsonObject data = p.createNestedObject("data");
    data["level"] = analogRead(A0); // Exemple
    data["battery"] = 3.9;
    
    sendLoRaMessage(doc);
}


void setup() {
    Serial.begin(115200);
    // ... init radio ...
    
    preferences.begin("module_cfg", false);
    myNodeId = preferences.getUChar("nodeId", 0);
    preferences.end();
    
    if (myNodeId == 0) {
        attemptJoin();
    }
}

void loop() {
    if (myNodeId != 0) {
        sendTelemetry();
    }
    
    // Écouter les commandes entrantes
    
    delay(60000); // Envoyer la télémétrie toutes les minutes
}
```

## Contribution

Les contributions sont les bienvenues ! Veuillez ouvrir une "issue" pour discuter des changements majeurs avant de soumettre une "pull request".

## Licence

Ce projet est sous licence MIT. Voir le fichier `LICENSE` pour plus de détails.
```
