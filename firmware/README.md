# 🦝 Ada Desktop Companion - Firmware

ESP32-S3 firmware for the Waveshare ESP32-S3-Touch-AMOLED-1.75 belly display.

## Hardware

- **Board:** Waveshare ESP32-S3-Touch-AMOLED-1.75
- **Display:** 466×466 AMOLED
- **Touch:** Capacitive (CST816D or similar)
- **Connectivity:** WiFi (2.4GHz)

## What It Does

1. Connects to WiFi
2. Opens WebSocket to Ada's server (`ws://<server>:8765`)
3. Receives scene commands (JSON)
4. Renders animations on the AMOLED display
5. Sends touch events back to server

## Arduino IDE Setup

### Board Configuration

1. Install ESP32 board package (v3.1.0+) via Board Manager
2. Select board: **ESP32S3 Dev Module**
3. Settings:
   - USB CDC On Boot: **Enabled**
   - Flash Size: **16MB (128Mb)**
   - PSRAM: **OPI PSRAM**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**

### Required Libraries

Install via Library Manager:

| Library | Purpose |
|---------|---------|
| `GFX_Library_for_Arduino` | Display driver (from Waveshare) |
| `ESP32_IO_Expander` | I/O expander for display |
| `SensorLib` | Touch controller driver |
| `XPowersLib` | Power management |
| `ArduinoWebsockets` | WebSocket client |
| `ArduinoJson` (v7+) | JSON parsing |

### WiFi Configuration

Create `config.h`:

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#define WIFI_SSID "YourNetwork"
#define WIFI_PASS "YourPassword"

// Ada server IP (the AIServer running ada_server.py)
#define ADA_SERVER_HOST "192.168.1.xxx"
#define ADA_SERVER_PORT 8765

#endif
```

## Scene Rendering

The firmware receives JSON scene commands and renders them:

```json
{
  "type": "scene",
  "scene": "ambient",
  "data": {"pattern": "breathing", "speed": 1.0, "density": 0.5},
  "mood": {"color_primary": "#00FFCC", "color_secondary": "#1a1a2e", "intensity": 0.5, "pulse_speed": 1.0},
  "transition": "fade",
  "duration_ms": 300
}
```

Each scene type has a dedicated renderer. Transitions are handled by crossfading between framebuffers.

## TODO

- [ ] Write the actual firmware (Arduino sketch)
- [ ] Scene renderers for all 8 scene types
- [ ] Smooth transition engine
- [ ] Touch event detection and reporting
- [ ] WiFi manager (captive portal for setup)
- [ ] OTA updates
- [ ] Audio passthrough via I2S (if adding speaker/mic to body)

## Pin Reference

Refer to the Waveshare wiki for the ESP32-S3-Touch-AMOLED-1.75 pinout.
Key pins are typically defined in the board's example code.

---

*The firmware is the skin. The server is the soul. Together, they're Ada.* 🦝
