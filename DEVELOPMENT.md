# Ada Particles - Development Checklist

## Vision
A fluid, dreamy particle system that makes Ada feel **alive**. Particles drift like they're suspended in water, coalesce into formations when expressing something, and respond to mood through color. 30+ FPS, polished, stable.

## Hardware
- Waveshare ESP32-S3-Touch-AMOLED-1.75
- 466×466 AMOLED, CO5300 driver via QSPI
- 8MB PSRAM, 16MB Flash
- Display orientation: USB-C on bottom (rotation TBD - will test)

---

## Implementation Checklist

### Phase 1: Core Foundation ✅ COMPLETE
- [x] **1.1** Create clean project structure
  - `firmware/` - Arduino project
  - `firmware/src/` - Core engine files
  - `tools/` - Python utilities
- [x] **1.2** Hardware abstraction
  - Display initialization with correct rotation (USB-C on bottom)
  - Brightness control
  - Basic GFX wrapper for our needs
- [x] **1.3** Fixed-point math utilities (`fixed_math.h`)
  - 16.16 fixed-point type
  - Fast multiply, divide
  - Fixed-point trig (sin/cos lookup table)
  - Conversion macros
- [x] **1.4** Simplex noise implementation (`noise.h`)
  - Port FastLED's inoise16 (16-bit fixed-point)
  - 2D and 3D variants
  - Fractal/octave noise function
  - Curl noise for fluid motion
- [x] **1.5** Framebuffer with fade trail (`framebuffer.h`)
  - PSRAM-backed 466×466 RGB565 buffer (~434KB)
  - `fade(factor)` - multiply all pixels by factor (trail effect)
  - `clear(color)` - fill with color
  - `pushToDisplay()` - DMA transfer to screen

### Phase 2: Particle Rendering ✅ COMPLETE
- [x] **2.1** Pre-rendered particle sprites (`sprites.h`)
  - Generate at boot time in PSRAM
  - 3 sizes: 8px, 16px, 24px diameter
  - Gaussian falloff for soft edges
  - Store as 8-bit alpha maps
- [x] **2.2** Additive blending renderer
  - `drawParticleAdditive(x, y, size, color, brightness)`
  - Proper RGB565 channel separation
  - Clamping to prevent overflow
  - Screen bounds clipping
- [x] **2.3** Batch rendering optimization
  - Skip fully off-screen particles early
  - Early exit for zero alpha

### Phase 3: Particle Physics ✅ COMPLETE
- [x] **3.1** Particle data structure
  - Position (fixed-point)
  - Velocity (fixed-point)
  - Target position (for formations, -1 if free)
  - Size index (0-2)
  - Brightness variation (0.8-1.2)
  - Phase offset (for variation)
- [x] **3.2** Core physics update
  - Noise-based wandering (organic flow via curl noise)
  - Soft center attraction (prevent drift)
  - Velocity damping
  - Position integration
- [ ] **3.3** Vector field system (DEFERRED - curl noise sufficient)
  - 32×32 grid covering screen
  - Each cell = flow direction
  - Particles sample local field
  - Field updates from server or procedurally

### Phase 4: Formations ✅ COMPLETE
- [x] **4.1** Formation data format
  - Procedurally generated based on index/total
  - Metadata: name, type enum
- [x] **4.2** Formation definitions
  - `IDLE` - no targets, pure wandering
  - `CLOUD` - fluffy cumulus shape
  - `SUN` - center cluster + radiating rays  
  - `RAIN` - vertical columns, particles drift down
  - `SNOW` - scattered, gentle drift
  - `HEART` - classic heart curve
  - `THINKING` - swirling vortex toward center
  - `WAVE` - sine wave pattern
  - `DISCONNECTED` - sad droopy shape
- [x] **4.3** Formation transitions
  - Smooth spring attraction to targets
  - Particles still "breathe" when formed
  - `setFormation(name, transitionMs)`
  - `clearFormation()` - return to idle

### Phase 5: Color & Mood System ✅ COMPLETE
- [x] **5.1** Mood parameters
  - `valence` (-1.0 to 1.0): negative=concerned, positive=happy
  - `arousal` (0.0 to 1.0): calm to alert
  - Smooth interpolation between states
- [x] **5.2** Color palette generation
  - Primary color from valence (blue ↔ cyan ↔ gold)
  - Brightness from arousal
  - Per-particle brightness jitter
- [x] **5.3** Color application
  - Apply mood color during render
  - Support future expansion (per-formation colors)

### Phase 6: State Management & WebSocket ✅ COMPLETE
- [x] **6.1** WiFi connection manager
  - Auto-reconnect with backoff
  - Status tracking
- [x] **6.2** WebSocket client
  - Connect to server
  - Handle JSON state messages
  - Send touch events back
- [x] **6.3** State protocol
  ```json
  {
    "mood": { "valence": 0.3, "arousal": 0.5 },
    "formation": "cloud",
    "transition_ms": 2000
  }
  ```
- [x] **6.4** Disconnected state
  - Detect connection loss
  - Show special "disconnected" visual
  - Dim colors, slower movement

### Phase 7: Touch Integration
- [ ] **7.1** Touch driver initialization (CST9217)
- [ ] **7.2** Touch event detection
  - Tap, hold, swipe
- [ ] **7.3** Particle interaction
  - Particles scatter from touch point
  - Ripple effect outward
- [ ] **7.4** Send events to server

### Phase 8: Polish & Optimization
- [ ] **8.1** Frame timing
  - Consistent 30+ FPS
  - Delta time calculation
  - Skip frames gracefully if behind
- [ ] **8.2** Memory profiling
  - Monitor PSRAM usage
  - Monitor heap fragmentation
- [ ] **8.3** Power optimization
  - Reduce brightness when idle
  - Sleep mode considerations
- [ ] **8.4** Error handling
  - Graceful degradation
  - Watchdog timer
  - Recovery from crashes

### Phase 9: Testing & Documentation
- [ ] **9.1** Test each formation visually
- [ ] **9.2** Test mood transitions
- [ ] **9.3** Test disconnection/reconnection
- [ ] **9.4** FPS benchmarking under load
- [ ] **9.5** Update README with setup instructions
- [ ] **9.6** Document WebSocket protocol

---

## File Structure (Target)

```
AdaDesktopCompanion/
├── firmware/
│   ├── ada_particles.ino      # Main entry point
│   ├── config.h               # Hardware pins, WiFi, constants
│   └── src/
│       ├── display.h          # Display abstraction
│       ├── display.cpp
│       ├── framebuffer.h      # PSRAM framebuffer with fade
│       ├── framebuffer.cpp
│       ├── fixed_math.h       # Fixed-point utilities
│       ├── noise.h            # Simplex noise
│       ├── noise.cpp
│       ├── sprites.h          # Pre-rendered particle sprites
│       ├── sprites.cpp
│       ├── particle.h         # Particle struct & pool
│       ├── particle_system.h  # Main particle engine
│       ├── particle_system.cpp
│       ├── formations.h       # Formation definitions
│       ├── formations.cpp
│       ├── mood.h             # Color/mood system
│       ├── mood.cpp
│       ├── state_client.h     # WebSocket state receiver
│       ├── state_client.cpp
│       ├── touch.h            # Touch handling
│       └── touch.cpp
├── tools/
│   └── formation_generator.py # Generate formation point arrays
├── DEVELOPMENT.md             # This file
├── README.md
└── SCREEN.md
```

---

## Constants & Tuning

```cpp
// Particle count
#define MAX_PARTICLES 400
#define DEFAULT_PARTICLES 300

// Physics (will tune)
#define WANDER_STRENGTH 0.3f
#define CENTER_PULL 0.001f
#define SPRING_K 0.05f
#define DAMPING 0.98f

// Rendering
#define FADE_FACTOR 0.92f  // Trail persistence (higher = longer trails)
#define TARGET_FPS 30

// Display
#define SCREEN_WIDTH 466
#define SCREEN_HEIGHT 466
#define DISPLAY_ROTATION 3  // TBD - USB-C on bottom
```

---

## Progress Log

### 2026-02-05
- Initial research and design
- Created development checklist
- Starting Phase 1 implementation

