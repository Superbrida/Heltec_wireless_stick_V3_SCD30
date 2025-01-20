# Heltec_wireless_stick_V3_SCD30
# CO2 Monitoring with Heltec Wireless Stick V3 and SCD30

![Project Image](https://example.com/project-image.jpg](https://github.com/Superbrida/Heltec_wireless_stick_V3_SCD30/blob/main/Image/IMG_1021.JPG)

## 📋 Project Description

This project utilizes the **Heltec Wireless Stick V3 (ESP32-S3)** microcontroller and the **SCD30 sensor** to monitor **CO2 levels**, temperature, and humidity. It features real-time data visualization on an **OLED display**, WiFi connectivity for remote monitoring, and battery status tracking.

### 🔧 Features

- 📡 **WiFi Connectivity** – Remote monitoring of environmental data.
- 📟 **OLED Display** – Real-time visualization of CO2, temperature, and humidity values.
- 🔋 **Battery Monitoring** – Displays battery level and enters deep sleep mode when power is low.
- 💤 **Deep Sleep Mode** – Optimized power consumption for long-term monitoring.
- ⚙️ **Touch Input** – Activation of display and WiFi status with touch detection.

---

## 🛠 Hardware Requirements

- **Heltec Wireless Stick V3 (ESP32-S3)**
- **SCD30 CO2 Sensor**
- **SSD1306 OLED Display (I2C)**
- Battery (Li-ion recommended)
- Miscellaneous: wires, resistors, PCB, etc.

### 📡 Pin Configuration

| Component | SDA Pin | SCL Pin |
|-----------|--------|--------|
| OLED Display | `SDA_OLED` | `SCL_OLED` |
| SCD30 Sensor | `45` | `46` |

---

## 🧩 Software Requirements

- **Arduino IDE** (with ESP32 board support)
- Required Libraries:
  - `WiFi.h`
  - `Wire.h`
  - `HT_SSD1306Wire.h` (for OLED)
  - `SparkFun_SCD30_Arduino_Library.h`


---

## 🚀 Installation & Setup

### 1. Clone the repository

```bash
git clone https://github.com/yourusername/CO2-Monitor.git
cd CO2-Monitor
