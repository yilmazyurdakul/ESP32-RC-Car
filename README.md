# SmartGarage – ESP32 Web-Controlled RC Platform

SmartGarage is an **ESP32-based Wi-Fi controlled vehicle firmware** featuring real-time WebSocket control, servo steering, PWM motor drive, lighting logic, battery monitoring, OTA firmware updates, and built-in safety mechanisms.

The device creates its own Wi-Fi Access Point and exposes a **mobile-friendly web interface** for controlling steering, throttle, lights, and firmware updates.

---

## 🚗 Features

- 📡 **Wi-Fi Access Point mode**
- 🕹️ **Real-time control via WebSocket**
- 🔄 **Servo steering (ESP32Servo)**
- ⚙️ **H-Bridge motor control with PWM**
- 🚦 **Headlights & intelligent tail-light logic**
- 🔋 **Battery voltage & percentage monitoring**
- 🛑 **Failsafe motor stop on signal loss**
- 🔁 **Over-the-Air (OTA) firmware updates**
- 💾 **SPIFFS for UI storage**
- 🔔 **Visual OTA status using tail lights**

---

## 🧠 System Overview

- **Control Interface:** Web UI (HTML served from ESP32)
- **Communication:** WebSocket (`/ws`)
- **Failsafe Timeout:** 200 ms
- **OTA Update Page:** `/update`
- **Battery ADC:** GPIO 34 (averaged & filtered)

---

## 🔌 Hardware Requirements

| Component | Description |
|--------|-------------|
| ESP32 | ESP32-WROOM or compatible |
| Servo | Standard 50 Hz RC servo |
| Motor Driver | H-Bridge (IN1 / IN2) |
| Battery | 2S Li-Ion / LiPo (up to 8.4V) |
| LEDs | Headlight + Tail light |
| Voltage Divider | For battery measurement |

---

## 📍 Pin Configuration

### Steering
| Function | GPIO |
|--------|------|
| Servo Signal | 18 |

### Motor (H-Bridge)
| Function | GPIO | PWM Channel |
|-------|------|-------------|
| Forward | 26 | 2 |
| Reverse | 27 | 3 |

### Lights
| Function | GPIO | PWM Channel |
|--------|------|-------------|
| Headlight | 33 | 4 |
| Tail Light | 25 | 5 |

### Battery
| Function | GPIO |
|--------|------|
| VIN Sense | 34 |

---

## 🌐 Wi-Fi Configuration

The ESP32 runs in **Access Point mode**:

