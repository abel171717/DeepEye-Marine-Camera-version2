# 🐟 DeepEye Marine Camera v2

<div align="center">

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Seeed%20XIAO-blue?style=for-the-badge&logo=espressif&logoColor=white)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.x-red?style=for-the-badge&logo=espressif&logoColor=white)
![Camera](https://img.shields.io/badge/OV2640-1600x1200-green?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Active-brightgreen?style=for-the-badge)

**An ultra-low-power marine camera system built on the ESP32-S3 (Seeed XIAO Sense), designed for autonomous underwater or marine field deployments. The device wakes from deep sleep on a DS3231 RTC alarm, captures a high-resolution JPEG image with the OV2640 camera, saves it timestamped to an SD card, and returns to deep sleep — all while maximising battery life.**

[Features](#-features) • [Hardware](#-hardware) • [Getting Started](#-getting-started) • [Configuration](#%EF%B8%8F-configuration) • [Architecture](#%EF%B8%8F-architecture) • [Power](#-power-optimizations)

</div>

---

## ✨ Features

| Feature | Details |
|---|---|
| 🌙 **Deep Sleep with RTC Wake** | DS3231 alarm triggers EXT0 wake-up; active window < 5 s |
| 📸 **High-Resolution Capture** | OV2640 at UXGA (1600 × 1200) JPEG, configurable quality |
| 💾 **SD Card Logging** | Timestamped JPEG files (`photo_<unix_seconds>.jpg`) via SPI FAT |
| 🔋 **Ultra-Low Power** | Peripheral pins set to Hi-Z before sleep; Wi-Fi stack fully stopped |
| 🕐 **Precise Scheduling** | DS3231 I²C RTC with battery-backed alarms; configurable interval |
| 📡 **Modular Design** | Independent `camera_module` and `sdcard_module` components |

---

## 🏗️ Architecture

```
DeepEye-Marine-Camera-version2/
├── main/
│   └── main.c                  # App entry point & deep-sleep orchestration
├── components/
│   ├── camera_module/          # OV2640 camera abstraction (init, capture, deinit)
│   │   ├── camera_module.c
│   │   └── include/camera_module.h
│   └── sdcard_module/          # SPI SD card abstraction (init, save JPEG, deinit)
│       ├── sdcard_module.c
│       └── include/sdcard_module.h
├── CMakeLists.txt
├── idf_component.yml           # Managed component dependencies
├── sdkconfig.defaults          # Pre-configured build settings
└── README.md
```

### Operational Flow

```
Power-on / RTC Alarm (EXT0)
        │
        ▼
  NVS Flash Init
        │
        ▼
  DS3231 Init → Read Time → Set Next Alarm (+2 min)
        │
        ▼
  Camera Init  (OV2640 · UXGA · JPEG)
        │
        ▼
  2 s Sensor Warm-up
        │
        ▼
  SD Card Init (SPI · FAT)
        │
        ▼
  Capture Frame → Save JPEG to SD
        │
        ▼
  Deinit Camera & SD Card
        │
        ▼
  Stop Wi-Fi Stack
        │
        ▼
  Set All Pins Hi-Z → Enable EXT0 Wake
        │
        ▼
  esp_deep_sleep_start()
```

---

## 🔧 Hardware

### Bill of Materials

| Component | Part | Notes |
|---|---|---|
| Microcontroller | Seeed XIAO ESP32-S3 Sense | Built-in OV2640 & PSRAM |
| RTC | DS3231 | I²C, battery-backed alarm |
| Storage | MicroSD card (FAT32) | Tested up to 32 GB |
| Power | LiPo / Li-Ion battery | 3.7 V; capacity per deployment need |

### Pin Assignments

#### DS3231 RTC (I²C)

| Signal | GPIO |
|---|---|
| SDA | GPIO 5 |
| SCL | GPIO 6 |
| INT / SQW | GPIO 2 *(EXT0 wake)* |

#### SD Card (SPI)

| Signal | GPIO |
|---|---|
| MISO | GPIO 8 |
| MOSI | GPIO 9 |
| CLK  | GPIO 7 |
| CS   | GPIO 21 |

#### OV2640 Camera

> Pins are managed internally by `camera_module` using the Seeed XIAO ESP32S3 Sense default mapping:
> XCLK: 10 · SIOD: 40 · SIOC: 39 · VSYNC: 38 · HREF: 47 · PCLK: 13 · D0–D7: 15, 16, 17, 18, 11, 12, 14, 48

---

## 🚀 Getting Started

### Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) installed and sourced
- Seeed XIAO ESP32-S3 Sense board
- DS3231 RTC module wired as per pin table above
- FAT32-formatted MicroSD card

### Clone & Build

```bash
# Clone the repository
git clone https://github.com/abelfernandes1717/DeepEye-Marine-Camera-version2.git
cd DeepEye-Marine-Camera-version2

# Set the target
idf.py set-target esp32s3

# Build (dependencies fetched automatically)
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

> **Windows:** replace `/dev/ttyUSB0` with your COM port, e.g. `COM5`.

---

## ⚙️ Configuration

### Capture Interval

In `main/main.c`, change the alarm offset to any desired interval:

```c
alarm.tm_min += 2;   // Wake every 2 minutes — adjust as needed
```

### Camera Resolution & Quality

```c
camera_config_params_t camera_params = {
    .frame_size   = FRAMESIZE_UXGA,   // 1600 x 1200
    .pixel_format = PIXFORMAT_JPEG,
    .jpeg_quality = 12,               // 0 = best quality, 63 = smallest file
    .fb_count     = 2
};
```

### SD Card Pins

```c
#define SD_MISO_GPIO   8
#define SD_MOSI_GPIO   9
#define SD_SCLK_GPIO   7
#define SD_CS_GPIO     21
#define MAX_SD_FILES   30
```

### Managed Dependencies (`idf_component.yml`)

```yaml
dependencies:
  espressif/esp32-camera: ^2.0.3
  jschwefel/esp-idf-ds3231: ^1.0.3
```

Dependencies are fetched automatically on `idf.py build`.

---

## 📂 SD Card Output

Each wake cycle produces one timestamped file:

```
/sdcard/
└── photo_1753876423.jpg   ← seconds since power-on (esp_timer_get_time / 1e6)
```

For absolute timestamps, cross-reference with DS3231 RTC output on the serial monitor.

---

## 📊 Serial Monitor Output

```
I (312)  RTC_CAM: Woke up from DS3231 alarm
I (450)  RTC_CAM: Current RTC time: 2026-08-18 12:00:00
I (451)  RTC_CAM: DS3231 alarm set for 12:02:00
I (1820) RTC_CAM: Initializing camera...
I (3900) RTC_CAM: Saved photo: /sdcard/photo_1753876423.jpg (87432 bytes)
I (3950) RTC_CAM: Disabling Wi-Fi to save power...
I (4060) RTC_CAM: Entering deep sleep...
```

---

## 🔋 Power Optimizations

| Technique | Effect |
|---|---|
| Deep sleep between captures | Dominant power draw reduced to µA range |
| All peripheral GPIOs set Hi-Z before sleep | Eliminates leakage through external pull resistors |
| Wi-Fi stack fully stopped (`esp_wifi_stop` + `esp_wifi_deinit`) | Removes RF power draw |
| Camera & SD card deinitialized before sleep | Prevents phantom current from peripherals |
| Single RTC alarm cycle — no polling | CPU active < 5 s per capture cycle |

---

## 🛠️ Extending the Project

| Idea | How |
|---|---|
| GPS tagging | Add a UART GPS module; prepend coordinates to the filename |
| LoRa uplink | Transmit thumbnail / metadata via LoRaWAN after capture |
| Motion trigger | Replace RTC alarm with a PIR or hydrophone interrupt |
| Cloud sync | Add a periodic Wi-Fi wake cycle to upload buffered photos |

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgements

- [Espressif ESP-IDF](https://github.com/espressif/esp-idf) — Embedded framework
- [espressif/esp32-camera](https://github.com/espressif/esp32-camera) — Camera driver
- [jschwefel/esp-idf-ds3231](https://components.espressif.com/components/jschwefel/esp-idf-ds3231) — DS3231 RTC component
- [Seeed Studio XIAO ESP32S3 Sense](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) — Reference hardware

---

<div align="center">
Made with ❤️ for marine field research
</div>
