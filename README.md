# LoRaWAN Gateway for ThingsBoard

## Overview

This project implements a robust, production-ready LoRaWAN gateway designed for seamless integration with the ThingsBoard IoT platform. Built on the powerful Heltec WiFi LoRa 32 (V3) ESP32 module, this gateway provides a secure and reliable solution for collecting sensor data from LoRa devices and forwarding it to your IoT dashboard.

The software is engineered to industrial-grade standards, emphasizing security, stability, and maintainability. It features a highly modular architecture based on FreeRTOS, enabling concurrent management of LoRa communication, MQTT connectivity, and a local OLED display.

## Key Features

- **Plug-and-Play Operation:** Designed for easy deployment with minimal configuration.
- **Enhanced Security:** Implements a robust security model with AES-128 encryption for LoRa payloads, a message counter to prevent replay attacks, and CRC32 checksums for data integrity.
- **ThingsBoard Integration:** Connects directly to the ThingsBoard platform using the MQTT protocol, enabling device provisioning, telemetry data visualization, and remote procedure calls (RPC).
- **FreeRTOS-Powered:** A multi-tasking architecture ensures responsive and reliable operation by dedicating tasks to specific functions:
  - **LoRaHandler:** Manages all incoming and outgoing LoRa communication.
  - **MqttHandler:** Handles connectivity and data exchange with the ThingsBoard MQTT broker.
  - **OledDisplay:** Provides real-time diagnostic information on a local OLED screen.
- **Device Management:** Includes a `DeviceManager` to handle device registration, authentication, and session management.
- **OLED Display Menu:** A user-friendly, multi-page diagnostic menu displays key system stats, including connection status, device information, and message counts.
- **Robust and Reliable:** The gateway is designed for long-term, stable operation, with built-in watchdog timers and automatic reconnection logic for both WiFi and MQTT.

## Software Architecture

The gateway's software is built on a modular, task-based architecture using FreeRTOS. This design ensures that critical functions operate independently and efficiently.

- **`main.cpp`:** Initializes hardware, creates FreeRTOS tasks, and starts the scheduler.
- **`LoRaHandler`:** This task is responsible for receiving, decrypting, and validating LoRa packets. It also handles the transmission of outgoing messages, such as acknowledgments and commands.
- **`MqttHandler`:** This task manages the WiFi connection and communication with the ThingsBoard MQTT broker. It publishes telemetry data received from the LoRa task and subscribes to RPC topics to receive commands from the dashboard.
- **`DeviceManager`:** This component is responsible for managing the registration and lifecycle of end-devices. It stores device information in Non-Volatile Storage (NVS) to persist data across reboots.
- **`OledDisplay`:** This task drives the OLED screen, providing a user interface for monitoring the gateway's status.

## Security Model

The gateway implements a comprehensive security model to protect against common threats:

- **AES-128 Encryption:** All LoRa payloads are encrypted using AES-128 in CBC mode, ensuring confidentiality.
- **Message Integrity:** A CRC32 checksum is appended to each message to prevent data corruption.
- **Replay Attack Prevention:** A message counter (`msgCtr`) is included in each LoRa message. The gateway validates that the incoming counter is greater than the last one received from that device.
- **Secure Credential Storage:** Sensitive information, such as WiFi credentials and MQTT tokens, is stored in a `credentials.h` file, which is excluded from version control.

## Getting Started

### Prerequisites

- **Hardware:**
  - Heltec WiFi LoRa 32 (V3)
- **Software:**
  - [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO IDE](https://platformio.org/platformio-ide/) extension.
  - [Git](https://git-scm.com/)

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/davidweb/loRa-Gateway.git
   cd loRa-gateway
   ```

2. **Configure Credentials:**
   - Rename `include/credentials.h.example` to `include/credentials.h`.
   - Edit `include/credentials.h` to add your WiFi SSID and password, as well as your ThingsBoard gateway token.

3. **Build and Upload:**
   - Open the project in Visual Studio Code.
   - The PlatformIO extension will automatically detect the `platformio.ini` file and download the required dependencies.
   - Build and upload the firmware to your Heltec board using the PlatformIO toolbar.

## Usage

Once the gateway is running, it will automatically connect to your WiFi network and the ThingsBoard MQTT broker. You can monitor its status on the OLED display.

### OLED Display

The display provides real-time diagnostics on three pages:

- **Home:** Shows the status of WiFi and MQTT connections, and the number of online devices.
- **Devices:** Lists all registered devices and their last-seen timestamps.
- **System Stats:** Displays uptime, message counts, and other performance metrics.

Press the hardware button on GPIO pin 0 to cycle through the pages.

## Troubleshooting

- **Compilation Errors:** If you encounter dependency issues, delete the `.pio` directory and rebuild the project. This will force PlatformIO to download fresh copies of all libraries.
- **Connection Issues:** Double-check your credentials in `include/credentials.h`. Verify that your WiFi network is available and that the ThingsBoard server is accessible.
- **LoRa Communication:** Ensure that your end-devices are configured with the correct frequency, spreading factor, and encryption keys.

