# Firmware des Modules de Terrain (AquaReservPro & WellguardPro)

Ce dossier contient le code source des différents modules capteurs/actionneurs ("esclaves") conçus pour communiquer avec la [passerelle LoRa](../gateway/README.md) principale. Chaque sous-dossier est un projet PlatformIO complet et autonome.

## Table des Matières
- [Architecture Commune](#architecture-commune)
- [Description des Modules](#description-des-modules)
  - [1. AquaReservPro (Module Réservoir)](#1-aquareservpro-module-réservoir)
  - [2. WellguardPro (Module Pompe de Puits)](#2-wellguardpro-module-pompe-de-puits)
- [Déploiement et Configuration](#déploiement-et-configuration)
  - [Prérequis](#prérequis)
  - [Procédure de Compilation](#procédure-de-compilation)
  - [Configuration Initiale du WiFi](#configuration-initiale-du-wifi)
- [Interface Web Locale](#interface-web-locale)
- [Protocole de Communication LoRa](#protocole-de-communication-lora)

## Architecture Commune

Tous les modules de ce projet partagent une architecture logicielle commune pour garantir la robustesse, la maintenabilité et une expérience utilisateur cohérente :

*   **FreeRTOS** : Le firmware est basé sur un système d'exploitation temps réel. Chaque fonctionnalité majeure (gestion LoRa, lecture des capteurs, serveur web) s'exécute dans une tâche dédiée, assurant un fonctionnement non bloquant et une grande réactivité.
*   **Persistance NVS** : Les informations de configuration critiques, notamment le `nodeId` LoRa attribué par la passerelle, sont sauvegardées en mémoire non-volatile (NVS). Un module n'effectue sa procédure d'adhésion qu'une seule fois.
*   **WiFiManager** : Pour la configuration initiale, chaque module crée un point d'accès WiFi captif nommé (`AquaReservPro-Setup` ou `WellguardPro-Setup`). L'utilisateur peut s'y connecter avec un téléphone ou un ordinateur pour renseigner les identifiants du WiFi local sans avoir à modifier le code.
*   **Interface Web Embarquée** : Chaque module expose une interface web moderne pour le contrôle et la supervision en local. Elle utilise des **WebSockets** pour des mises à jour des données en temps réel, sans rechargement de la page.
*   **Logique "Plug and Play"** : Les modules implémentent le protocole de communication LoRa défini pour l'écosystème, incluant la demande d'adhésion (`JOIN_REQUEST`) et la validation de l'intégrité des messages par **CRC32**.

## Description des Modules

### 1. AquaReservPro (Module Réservoir)

*   **Rôle** : Surveiller le niveau d'eau dans un réservoir.
*   **Capteurs/Actionneurs** :
    *   1x Capteur de niveau de type contact (flotteur).
*   **Fonctionnalités Clés** :
    *   **Logique de confirmation temporelle** : Pour éviter les faux positifs dus aux remous, un changement d'état du capteur n'est validé que s'il reste stable pendant une durée configurable (`LEVEL_CONFIRMATION_MS`).
    *   Envoi de télémétrie LoRa à chaque changement d'état confirmé.
    *   Interface web minimaliste affichant l'état "Plein" ou "Vide" en temps réel.
*   **Configuration** : Fichier `AquaReservPro/include/config.h`.

### 2. WellguardPro (Module Pompe de Puits)

*   **Rôle** : Contrôler une pompe de puits et surveiller les paramètres environnementaux et électriques.
*   **Capteurs/Actionneurs** :
    *   1x Module relais pour activer/désactiver la pompe.
    *   1x Capteur de température et d'humidité (DHT22).
    *   1x Capteur de tension (via pont diviseur sur une entrée analogique).
    *   1x Capteur de pression de type contact (pressostat).
*   **Fonctionnalités Clés** :
    *   **Contrôle Bidirectionnel** : La pompe peut être activée depuis l'interface web locale ou via une commande LoRa reçue de la passerelle.
    *   **Gestion Concurrente Sécurisée** : L'accès à l'état de la pompe est protégé par un **sémaphore FreeRTOS** pour éviter les conflits entre les commandes web, les commandes LoRa et la lecture des capteurs.
    *   Envoi périodique de la télémétrie (température, humidité, tension, pression).
    *   Interface web complète affichant toutes les données des capteurs et permettant le contrôle de la pompe.
*   **Configuration** : Fichier `WellguardPro/include/config.h`.

## Déploiement et Configuration

### Prérequis
*   Visual Studio Code avec l'extension PlatformIO.

### Procédure de Compilation

Ce dépôt étant un **monorepo**, la gestion de projet se fait de manière centralisée.

1.  Ouvrez le dossier racine `loRa-Gateway` dans VSCode.
2.  Cliquez sur l'icône PlatformIO (tête de fourmi) dans la barre latérale.
3.  Dans la section **"Project Tasks"**, une liste déroulante vous permettra de choisir l'environnement sur lequel travailler : `AquaReservPro` ou `WellguardPro`.
4.  Sélectionnez le module désiré. Vous pouvez alors utiliser les commandes `Build`, `Upload`, et `Monitor` pour ce projet spécifique.
5.  Avant de téléverser, personnalisez les pins et les paramètres dans le fichier `include/config.h` du module concerné.

### Configuration Initiale du WiFi

Lors du premier démarrage, le module ne connaîtra pas les identifiants de votre réseau WiFi.

1.  Mettez le module sous tension.
2.  Avec votre téléphone ou ordinateur, cherchez les réseaux WiFi disponibles.
3.  Connectez-vous au point d'accès nommé **`AquaReservPro-Setup`** ou **`WellguardPro-Setup`**.
4.  Une page de configuration (portail captif) devrait s'ouvrir automatiquement. Si ce n'est pas le cas, ouvrez un navigateur et allez à l'adresse `192.168.4.1`.
5.  Sélectionnez votre réseau WiFi local, entrez le mot de passe, et sauvegardez.
6.  Le module va redémarrer et se connecter automatiquement à votre réseau.

## Interface Web Locale

Une fois le module connecté à votre réseau WiFi, vous pouvez accéder à son interface de contrôle locale.

1.  Récupérez l'adresse IP du module. Vous pouvez la trouver :
    *   Dans le **Moniteur Série** de PlatformIO au démarrage.
    *   Dans la liste des clients de l'interface d'administration de votre routeur.
2.  Ouvrez un navigateur web sur le même réseau et entrez cette adresse IP.

L'interface se met à jour en temps réel grâce aux WebSockets.

## Protocole de Communication LoRa

Les modules respectent le protocole défini par la passerelle. Tous les messages sont des objets JSON avec un payload `p` et un checksum `c` (CRC32).

**1. Demande d'Adhésion (`JOIN_REQUEST`)** (Module -> Passerelle)
Envoyé au démarrage si le module n'a pas de `nodeId`.
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

**2. Télémétrie (`TELEMETRY`)** (Module -> Passerelle)
Envoyé périodiquement ou lors d'un changement d'état.
```json
// Exemple de WellguardPro
{
  "p": {
    "type": "TELEMETRY",
    "nodeId": 5,
    "data": {
      "temperature": 25.4,
      "humidity": 65.2,
      "voltage": 231.5,
      "pressure_ok": true
    }
  },
  "c": 1234567890 // CRC32 calculé
}
```

**3. Commande (`CMD`)** (Passerelle -> Module)
Message reçu par un module pour déclencher une action.
```json
// Exemple reçu par WellguardPro
{
  "p": {
    "type": "CMD",
    "method": "setPump",
    "params": { "state": true }
  },
  "c": 9876543210 // CRC32 calculé
}
```
