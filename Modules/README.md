# Firmware des Modules de Terrain (AquaReservPro & WellguardPro)

Ce dossier contient le code source des différents modules capteurs/actionneurs ("esclaves") conçus pour communiquer avec la [passerelle LoRa](../README.md) principale. Chaque sous-dossier est un projet PlatformIO complet et autonome.

**NOTE IMPORTANTE :** Ce code a été refactorisé pour atteindre une qualité industrielle, en mettant l'accent sur la sécurité et la robustesse. Toutes les communications LoRa sont maintenant chiffrées avec AES-128 et incluent des mécanismes de protection contre les attaques par rejeu.

## Table des Matières
- [Architecture Commune](#architecture-commune)
- [Description des Modules](#description-des-modules)
  - [1. AquaReservPro (Module Réservoir)](#1-aquareservpro-module-réservoir)
  - [2. WellguardPro (Module Pompe de Puits)](#2-wellguardpro-module-pompe-de-puits)
- [Déploiement et Configuration](#déploiement-et-configuration)
  - [Prérequis](#prérequis)
  - [Procédure de Compilation](#procédure-de-compilation)
- [Interface Web Locale](#interface-web-locale)
- [Protocole de Communication LoRa Sécurisé](#protocole-de-communication-lora-sécurisé)

## Architecture Commune

Tous les modules de ce projet partagent une architecture logicielle commune pour garantir la robustesse, la maintenabilité et une expérience utilisateur cohérente :

*   **FreeRTOS** : Le firmware est basé sur un système d'exploitation temps réel. Chaque fonctionnalité majeure (gestion LoRa, lecture des capteurs, serveur web) s'exécute dans une tâche dédiée, assurant un fonctionnement non bloquant et une grande réactivité.
*   **Persistance NVS** : Les informations de configuration critiques, notamment le `nodeId` LoRa et le compteur de messages `msgCtr`, sont sauvegardées en mémoire non-volatile (NVS). Un module n'effectue sa procédure d'adhésion qu'une seule fois et reprend son état après un redémarrage.
*   **Configuration Statique** : Pour une robustesse maximale en production, la configuration WiFi est maintenant codée en dur dans le fichier `credentials.h`.
*   **Interface Web Embarquée** : Chaque module expose une interface web moderne pour le contrôle et la supervision en local. Elle utilise des **WebSockets** pour des mises à jour des données en temps réel, sans rechargement de la page.
*   **Logique "Plug and Play" Sécurisée** : Les modules implémentent le protocole de communication sécurisé de la passerelle.

## Description des Modules

### 1. AquaReservPro (Module Réservoir)

*   **Rôle** : Surveiller le niveau d'eau dans un réservoir.
*   **Fonctionnalités Clés** :
    *   **Logique de confirmation temporelle** : Pour éviter les faux positifs, un changement d'état du capteur n'est validé que s'il reste stable pendant une durée configurable (`LEVEL_CONFIRMATION_MS`).
    *   Envoi de télémétrie LoRa chiffrée à chaque changement d'état.
    *   Interface web minimaliste affichant l'état "Plein" ou "Vide" en temps réel.
*   **Configuration** : Fichiers `AquaReservPro/include/config.h` et `AquaReservPro/include/credentials.h`.

### 2. WellguardPro (Module Pompe de Puits)

*   **Rôle** : Contrôler une pompe de puits et surveiller les paramètres environnementaux et électriques.
*   **Fonctionnalités Clés** :
    *   **Contrôle Bidirectionnel Sécurisé** : La pompe peut être activée depuis l'interface web locale ou via une commande LoRa chiffrée reçue de la passerelle. Le module envoie un acquittement (ACK) pour confirmer la réception de la commande LoRa.
    *   **Gestion Concurrente Sécurisée** : L'accès à l'état de la pompe est protégé par un **sémaphore FreeRTOS**.
    *   Envoi périodique de la télémétrie chiffrée.
    *   Interface web complète affichant toutes les données des capteurs et permettant le contrôle de la pompe.
*   **Configuration** : Fichiers `WellguardPro/include/config.h` et `WellguardPro/include/credentials.h`.

## Déploiement et Configuration

### Prérequis
*   Visual Studio Code avec l'extension PlatformIO.

### Procédure de Compilation

1.  Ouvrez le dossier racine `loRa-Gateway` dans VSCode.
2.  **Configurez les informations d'identification** pour le module que vous souhaitez compiler en modifiant le fichier `Modules/NOM_DU_MODULE/include/credentials.h`. Vous devez y renseigner votre SSID et mot de passe WiFi.
3.  Dans la barre de statut de VSCode, cliquez sur l'environnement PlatformIO (par ex. `Default`) et choisissez l'environnement que vous souhaitez compiler (`heltec_wifi_lora_32_V3` sous le projet `AquaReservPro` ou `WellguardPro`).
4.  Utilisez les commandes `Build`, `Upload`, et `Monitor` de PlatformIO pour compiler et téléverser le firmware.

## Interface Web Locale

Une fois le module connecté à votre réseau WiFi, vous pouvez accéder à son interface de contrôle locale.

1.  Récupérez l'adresse IP du module. Vous pouvez la trouver :
    *   Dans le **Moniteur Série** de PlatformIO au démarrage.
    *   Dans la liste des clients de l'interface d'administration de votre routeur.
2.  Ouvrez un navigateur web sur le même réseau et entrez cette adresse IP.

L'interface se met à jour en temps réel grâce aux WebSockets.

## Protocole de Communication LoRa Sécurisé

Les modules respectent le protocole sécurisé défini par la passerelle. Tous les messages sont des objets JSON avec un payload chiffré `p` et un checksum `c` (CRC32).

**Structure du message :**
```json
{
  "p": "<Base64(AES-128(payload_json))>",
  "c": "<CRC32(payload_json)>"
}
```

**Contenu du payload déchiffré :**

**1. Demande d'Adhésion (`JOIN_REQUEST`)** (Module -> Passerelle)
```json
{
  "type": "JOIN_REQUEST",
  "mac": "AA:BB:CC:11:22:33",
  "devType": "WELL_PUMP_STATION"
}
```

**2. Télémétrie (`TELEMETRY`)** (Module -> Passerelle)
```json
{
  "type": "TELEMETRY",
  "nodeId": 5,
  "msgCtr": 123,
  "data": { ... }
}
```

**3. Commande (`CMD`)** (Passerelle -> Module)
```json
{
  "type": "CMD",
  "nodeId": 5,
  "msgId": 456,
  "method": "setPump",
  "params": { "state": true }
}
```

**4. Acquittement (`ACK`)** (Module -> Passerelle)
```json
{
  "type": "ACK",
  "nodeId": 5,
  "msgId": 456,
  "msgCtr": 124
}
```
