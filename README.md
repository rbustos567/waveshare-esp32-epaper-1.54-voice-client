# ESP32-S3 e-Paper 1.54 Voice Commander Client

Firmware for the Waveshare ESP32-S3 e-Paper 1.54" development board, designed for low-power operation, robust FreeRTOS task management, NVS persistence, NTP synchronization, and secure HTTP audio streaming to a remote server backend.

## Features

- **Low-Power & Power Holding Control:** Keeps the circuit powered via a latching GPIO pin (GPIO17) to run independently on LiPo battery power.
- **Robust Error Handling:** Structured esp_err_t error propagation across system modules with professional ESP-IDF logging (ESP_LOGx) and clean fallback behaviors.
- **E-Paper Display (EPD) UI:** Multi-state UI manager supporting a Splash Screen, hierarchical menus, configuration editors (Timezone & Sleep Timeout), and real-time partial/full rendering.
- **Button Event Processing:** Dedicated FreeRTOS task with hardware debouncing (GPIO0) supporting single-click navigation and double-click actions.
- **Voice Recording & HTTP Streaming:** Captures raw audio via I2S, streams binary payloads via HTTPS/HTTP POST to a backend service, and plays back server responses.
- **Network & Time Management:** Automatic Wi-Fi connection handling and SNTP time synchronization with robust callback flags to prevent race conditions.

## Hardware Requirements

- **Development Board:** Waveshare ESP32-S3 e-Paper 1.54" (incorporating an ESP32-S3 microcontroller and an integrated SPI E-Paper display).
- **Power Source:** USB Type-C connection (for flashing/debugging) or a 3.7V LiPo battery connected to the battery header.
- **Peripherals:** Integrated BOOT button (GPIO0) and power latch pin (GPIO17).

## Prerequisites & Environment Setup

Ensure you have the official ESP-IDF (v5.x recommended) framework installed and configured on your development machine.

1. Install ESP-IDF prerequisites following the Espressif Documentation.
2. Set up the environment variables:
```bash
. $HOME/esp/esp-idf/export.sh
```

## Configuration (menuconfig)

Before compiling, configure your project-specific parameters (such as Wi-Fi credentials, server endpoints, or partition tables if applicable):
```bash
idf.py menuconfig
```
1. Navigate to your custom component settings to verify network options.

2. Save and exit.
