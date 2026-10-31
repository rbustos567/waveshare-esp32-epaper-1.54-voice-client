# ESP32-S3 e-Paper 1.54 Voice Commander Client

Firmware for the Waveshare ESP32-S3 e-Paper 1.54" development board, designed for low-power operation, robust FreeRTOS task management, NVS persistence, NTP synchronization, and secure HTTP audio streaming to a remote server backend.

<img width="4096" height="3072" alt="IMG20261024164440" src="https://github.com/user-attachments/assets/d87208ca-fac7-4133-b2b1-085c01697363" />

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

1. Install ESP-IDF prerequisites following the [Espressif Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html).
2. Set up the environment variables:
```bash
. $HOME/esp/esp-idf/export.sh
```

## Configuration (menuconfig & Source Defines)
Before compiling, you need to configure your network credentials and server endpoint:

1. Open main/wifi_config.h and define your Wi-Fi SSID and password. By default, you can provide two sets of SSID/password. If you only provide one, do not forget to set INITIAL_WIFI_COUNT to 1. The first will be the default one.
```bash
#define INITIAL_WIFI_COUNT      2

#define INITIAL_WIFI_SSID_0 "YourHome"
#define INITIAL_WIFI_PASS_0 "YourHomePass"

#define INITIAL_WIFI_SSID_1 "YourMobile"
#define INITIAL_WIFI_PASS_2 "YourMobilePass"
```
2. Open main/servers_config.h and configure your backend server IP address. By default, you can provide two IP addresses. If you only provide one, do not forget to set INITIAL_SERVER_COUNT to 1. The first will be the default one.
```bash
#define INITIAL_SERVER_COUNT      2

#define INITIAL_SERVER_URL_0      "https://server0"
#define INITIAL_SERVER_URL_1      "https://server1"
```
3. Run the project configuration tool to adjust any other system parameters if needed:
```bash
idf.py menuconfig
```
4. Save and exit after verifying your configurations.


## Building and Flashing
Follow these steps to build, flash, and monitor the firmware on your Waveshare ESP32-S3 e-Paper device:

1. Connect the Board: Plug your ESP32-S3 board into your computer via the USB port.
2. Set the Target (if not already configured):
```bash
idf.py set-target esp32s3
```
3. Build the Project:
Compile the source code to generate the binaries:
```bash
idf.py build
```
4. Flash to the Device:
Replace /dev/ttyUSB0 with your actual serial port (e.g., /dev/COM3 on Windows or /dev/ttyACM0 on Linux):
```bash
idf.py -p /dev/ttyUSB0 flash
```
5. Monitor Serial Output:
Open the serial monitor to view real-time logs and debug output:
```bash
idf.py -p /dev/ttyUSB0 monitor
```
(Tip: Press Ctrl+] to exit the monitor).

## Project Structure

- **main/main.cpp:** Entry point (app_main), system boot initialization, and wakeup cause handling.
- **main/ui_control.c:** UI state machine and button event queue processing tasks.
- **main/api_client.c:** HTTP client communication, multipart/binary payload transmission, and header parsers.
- **main/time_manager.c:** SNTP time synchronization and timezone persistence via NVS.
- **main/power_manager.c:** Sleep timeout management and battery monitoring.

## Device Usage & Navigation

The device uses the physical BOOT button (located next to the micro SD card slot) as the primary input control, utilizing a combination of single and double clicks managed by a debounced FreeRTOS task.
- **Splash Screen to Main Menu:**
  - Double-click the BOOT button from the initial informational splash screen to enter the main menu.
- **Menu Navigation:**
  - **Single Click:** Cycle forward through the available menu options.
  - **Double Click:** Select and enter the currently highlighted menu option or submenu.
- **Audio Command Workflow:**
  - Navigate to the **Audio Command** option and double-click to select it; this brings up the recording screen.
  - **Double Click:** Initiates the recording state. Wait until the screen displays **STATUS: Recording...** before starting to speak.
  - **Double Click (again):** Stops recording and automatically sends the audio payload via HTTP POST to your remote server backend, waiting for and displaying the response.
  - **Single Click:** Cancels the recording/waiting state and exits back to the previous menu.

## Gallery

<img width="4096" height="3072" alt="IMG20261024164456" src="https://github.com/user-attachments/assets/067e3838-ff0b-4b7d-ae0e-f604f2522b05" />
<img width="4096" height="3072" alt="IMG20261024164504" src="https://github.com/user-attachments/assets/d288e248-c45f-44a6-ad5d-02f7cbd67fa3" />
<img width="4096" height="3072" alt="IMG20261024164529" src="https://github.com/user-attachments/assets/fb48693a-3811-425a-ab82-488d61afd60d" />
<img width="4096" height="3072" alt="IMG20261024164603" src="https://github.com/user-attachments/assets/279c06ca-6627-466a-a885-ed889b3e672c" />
<img width="4096" height="3072" alt="IMG20261024164621" src="https://github.com/user-attachments/assets/2d339188-aff8-46fb-b58b-d6b1384d5dd7" />
<img width="4096" height="3072" alt="IMG20261024164634" src="https://github.com/user-attachments/assets/4d54fd36-7952-4560-ac6e-b1f981abf6fd" />


## License

This project is open-source and available under the terms of the MIT License.
