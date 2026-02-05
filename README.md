# Ada Desktop Companion

A dreamy particle display system for Ada's physical embodiment. Particles drift like they're suspended in water, coalesce into formations when Ada wants to express something, and respond to mood through dynamic colors.

## Hardware

- **Waveshare ESP32-S3-Touch-AMOLED-1.75**
  - 466×466 AMOLED display
  - CO5300 driver via QSPI
  - ESP32-S3 with 8MB PSRAM
  - Capacitive touch
  - Dual microphones + speaker

## Features

### Particle System
- 200-400 soft-glow particles with Gaussian falloff
- Simplex noise-driven organic motion (like floating in water)
- Trail effect via framebuffer fade (dreamy persistence)
- Additive blending for natural glow when particles overlap
- Fixed-point math for smooth 30+ FPS performance

### Formations
Particles can form recognizable shapes:
- ☁️ **Cloud** - Fluffy cumulus shape
- ☀️ **Sun** - Center cluster with radiating rays
- 🌧️ **Rain** - Vertical columns drifting down
- ❄️ **Snow** - Scattered gentle drift
- ❤️ **Heart** - Classic heart curve
- 💭 **Thinking** - Swirling vortex
- 〰️ **Wave** - Sine wave pattern

### Mood-Driven Colors
Colors respond to emotional state:
- **Valence** (−1 to +1): Blue (concerned) → Cyan (neutral) → Gold (happy)
- **Arousal** (0 to 1): Dim/calm → Bright/energetic

### Disconnected State
When WiFi connection is lost:
- Particles form a sad droopy shape
- Colors dim to blue-gray
- Automatically recovers when connection restored

## Quick Start

### 1. Install Arduino IDE & Libraries

Required libraries (install via Library Manager):
- `Arduino_GFX_Library` (or use Waveshare's modified version)
- `ArduinoWebsockets`
- `ArduinoJson`

### 2. Configure WiFi

Edit `firmware/config.h`:
```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define SERVER_HOST "your_server_ip"
#define SERVER_PORT 8765
```

### 3. Upload

1. Open `firmware/ada_particles.ino` in Arduino IDE
2. Select board: **ESP32S3 Dev Module**
3. Enable: **USB CDC On Boot**
4. Set Flash Size: **16MB**
5. Set PSRAM: **OPI PSRAM**
6. Upload

### 4. Display Orientation

The display is configured with USB-C port on the **bottom**.
If your orientation is different, adjust `DISPLAY_ROTATION` in `config.h`:
- `0` = Default (USB-C left)
- `1` = 90° clockwise
- `2` = 180°
- `3` = 270° clockwise (USB-C bottom) ← Current setting

## WebSocket Protocol

The display receives state updates via WebSocket:

```json
{
  "type": "state",
  "mood": {
    "valence": 0.3,
    "arousal": 0.5
  },
  "formation": "cloud",
  "transition_ms": 2000,
  "particle_count": 300
}
```

### Formation Names
`idle`, `cloud`, `sun`, `rain`, `snow`, `heart`, `thinking`, `wave`

### Brightness Control
```json
{
  "type": "config",
  "brightness": 200
}
```

## Architecture

```
firmware/
├── ada_particles.ino      # Main entry point
├── config.h               # Hardware & tuning constants
└── src/
    ├── fixed_math.h/cpp   # 16.16 fixed-point arithmetic
    ├── noise.h/cpp        # Simplex noise for organic motion
    ├── sprites.h/cpp      # Pre-rendered soft particle sprites
    ├── framebuffer.h/cpp  # PSRAM framebuffer with fade trail
    ├── particle.h/cpp     # Particle data structure & pool
    └── particle_system.h/cpp  # Main engine
```

## Performance

- **Target:** 30 FPS minimum
- **Typical:** 35-45 FPS with 300 particles
- **Memory:** ~700KB PSRAM used (framebuffer + particles + sprites)

## Tuning

Key parameters in `config.h`:

```cpp
// Particle behavior
#define WANDER_STRENGTH 0.4f    // Noise influence on motion
#define CENTER_PULL 0.0008f     // Attraction to center
#define SPRING_K 0.06f          // Formation attraction strength
#define DAMPING 0.97f           // Velocity damping

// Visuals
#define FADE_FACTOR 0.92f       // Trail persistence (higher = longer trails)
#define DEFAULT_PARTICLE_COUNT 300
```

## License

MIT

## Credits

- Ada & Chase
- Inspired by [Codrops dreamy particle tutorial](https://tympanus.net/codrops/2024/12/19/crafting-a-dreamy-particle-effect-with-three-js-and-gpgpu/)
- Simplex noise based on Stefan Gustavson's work
