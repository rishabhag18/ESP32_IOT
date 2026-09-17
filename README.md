# ESP32_IOT
Here is a complete, professional `README.md` tailored specifically for your dual-core architecture. You can copy and paste this directly into your GitHub repository.

It covers all the key engineering points, the strict GPIO mapping, the local fallback dashboard, and a deep dive into the relay logic configuration you requested.

---

# 🚀 Smart Home Hub - ESP32 Dual-Core RainMaker

A commercial-grade, ultra-fast smart home firmware for the ESP32. This project utilizes **FreeRTOS Dual-Core Execution** to completely isolate physical wall switches from network latency, ensuring zero-lag physical control even if the Wi-Fi drops or the cloud crashes.

It integrates seamlessly with **ESP RainMaker** for remote iOS/Android control and features an always-on **Local Fallback Web Dashboard** with a built-in QR pairing generator.

## ✨ Key Features

* **True Dual-Core Architecture:**
* **Core 0:** Manages Wi-Fi, the AWS RainMaker MQTT connection, and the Local Web Server.
* **Core 1:** Dedicated exclusively to a high-speed custom hardware scanner for physical wall switches.


* **Zero-Latency Hardware Scanner:** Replaces bloated button libraries with a custom, non-blocking debounce algorithm. Never misses a switch flip.
* **Cloud Rate-Limiting Queue:** Prevents `errno=104` and `errno=119` cloud flooding crashes by queuing and throttling rapid physical switch bounces before sending them to AWS.
* **Always-On Local Dashboard:** Broadcasts its own Wi-Fi network (`Robotics_Lab_Local`). Features a responsive iOS-style toggle UI and an embedded provisioning QR code.
* **Bulletproof GPIO Mapping:** Specifically mapped for 30-pin ESP32 DevKit V1 boards to permanently avoid boot-crashing strapping pins (0, 2, 5, 12, 15).

---

## ⚙️ Configuration Guide: `RELAY_ACTIVE_LOW`

One of the most critical settings in the code is the relay logic toggle. You must set this based on the physical hardware of your relay module.

```cpp
const bool RELAY_ACTIVE_LOW = true;  // Set to true for Active-Low relays, false for Active-High

```

### What does this mean?

* **If set to `true` (Active-Low):** The ESP32 will output `0V (LOW)` to turn the relay **ON**, and `3.3V (HIGH)` to turn the relay **OFF**.
* *Why?* Most commercial opto-isolated relay boards (the ones with blue or black cubes and PC817 chips) are wired to sink current. They require a `LOW` signal to activate the electromagnet.


* **If set to `false` (Active-High):** The ESP32 will output `3.3V (HIGH)` to turn the relay **ON**, and `0V (LOW)` to turn the relay **OFF**.
* *Why?* Use this if you are using bare transistors, MOSFETs, or specific solid-state relays that trigger on a positive voltage.



The firmware automatically flips the internal logic for the physical switches, the local web UI, and the RainMaker cloud so that your code remains clean regardless of your hardware type.

---

## 🔌 Safe Hardware Wiring Map (30-Pin ESP32)

To avoid "strapping pin" conflicts that prevent the ESP32 from booting, wire your relays and switches strictly to these tested pins:

| Channel | Relay Output Pin (ESP32) | Switch Input Pin (ESP32) |
| --- | --- | --- |
| **Relay 1** | GPIO 23 | GPIO 13 |
| **Relay 2** | GPIO 22 | GPIO 14 |
| **Relay 3** | GPIO 21 | GPIO 27 |
| **Relay 4** | GPIO 19 | GPIO 33 |
| **Relay 5** | GPIO 18 | GPIO 4 |
| **Relay 6** | GPIO 26 | GPIO 16 (RX2) |
| **Relay 7** | GPIO 25 | GPIO 17 (TX2) |
| **Relay 8** | GPIO 32 | GPIO 34 *(Requires external 10k pull-up)* |

> **Note on Switch Type:** This code is designed for **Standard Latched Wall Switches** (modular click-switches), not momentary push-buttons. Strict logic applies: Switch Down = ON, Switch Up = OFF.

---

## 📱 App Configuration (ESP RainMaker)

1. Download the **ESP RainMaker** app from the [App Store (iOS)](https://apps.apple.com/us/app/esp-rainmaker/id1497491540) or [Google Play (Android)](https://play.google.com/store/apps/details?id=com.espressif.rainmaker).
2. Create an account and log in.
3. Power on your ESP32. Ensure your phone's **Bluetooth** is turned on.
4. Open the RainMaker app and click **Add Device** (or the `+` icon).
5. Scan the QR code.
* *Note: If you don't have the Serial Monitor open to scan the QR code, connect to the local fallback Wi-Fi (see below) to scan the QR code directly from your browser!*


6. If prompted for a PIN/POP code manually, enter: **`Robotics123`**
7. Select your home Wi-Fi network and enter your password. The device will provision and appear in your app.

---

## 🌐 Emergency Local Fallback Web UI

If your internet goes down, AWS RainMaker crashes, or your phone app is unresponsive, you can control your entire system locally with zero latency.

1. Open your phone or laptop's Wi-Fi settings.
2. Connect to the ESP32's internal network:
* **SSID:** `Robotics_Lab_Local`
* **Password:** `Robotics123`


3. Open any web browser and go to: **`http://192.168.4.1`**
4. The Techvein Dashboard will load instantly. You can toggle relays using the iOS-style switches, or scan the embedded QR code to re-pair the device to a new phone.

---

## 🛠️ Installation & Compilation

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the **ESP32 Board Manager** (Version 2.x or 3.x).
3. Ensure the ESP RainMaker libraries are installed via the board manager.
4. Select **ESP32 Dev Module** from the Boards menu.
5. Set the partition scheme to **RainMaker** or **Huge APP (3MB No OTA)** if you run out of flash space.
6. Click **Upload**. *(If you get a `Write timeout` error, hold the physical `BOOT` button on the ESP32 when you see "Connecting..." in the console).*

---
