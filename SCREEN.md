# 🖥️ Screen Protocol & Design Spec

## Display: Waveshare ESP32-S3-Touch-AMOLED-1.75
- **Resolution:** 466×466 pixels
- **Type:** AMOLED (true blacks, vibrant colors)
- **Shape:** Square (perfect for centered content)
- **Touch:** Capacitive (tap, swipe, hold)
- **Connection:** WiFi → WebSocket to server (port 8765)

## Communication Protocol

### Server → ESP32 (Scene Commands)

All messages are JSON over WebSocket:

```json
{
  "type": "scene",
  "scene": "ambient",
  "data": { ... },
  "mood": {
    "color_primary": "#00FFCC",
    "color_secondary": "#1a1a2e",
    "intensity": 0.5,
    "pulse_speed": 1.0
  },
  "transition": "fade",
  "duration_ms": 300
}
```

### ESP32 → Server (Touch Events)

```json
{
  "type": "touch",
  "event": "tap|swipe_up|swipe_down|swipe_left|swipe_right|hold",
  "x": 233,
  "y": 233,
  "duration_ms": 150
}
```

## Scene Types

### 1. `ambient` — Idle State
The default. Ada is vibing. Raccoon energy at rest.

```json
{
  "scene": "ambient",
  "data": {
    "pattern": "breathing|sparkles|aurora|fireflies|constellation",
    "speed": 1.0,
    "density": 0.5
  }
}
```

**Patterns:**
- **breathing** — Slow color pulse from center outward (like a heartbeat)
- **sparkles** — Random twinkling points of light
- **aurora** — Flowing northern-lights ribbons
- **fireflies** — Floating warm particles with trails
- **constellation** — Connected dots forming shapes, slowly rotating

### 2. `weather` — Weather Widget
Shows current conditions with animated backgrounds.

```json
{
  "scene": "weather",
  "data": {
    "condition": "sunny|cloudy|rain|snow|storm|fog|wind",
    "temp_f": 72,
    "temp_c": 22,
    "humidity": 65,
    "description": "Partly cloudy",
    "location": "Flint, MI",
    "icon": "⛅"
  }
}
```

**Animations per condition:**
- **sunny** — Warm gradient, floating sun rays
- **rain** — Falling droplets with ripple effects
- **snow** — Gentle snowflakes drifting
- **storm** — Dark clouds, lightning flashes
- **fog** — Drifting translucent layers

### 3. `code_rain` — Matrix/Hacker Mode
Because every raccoon needs a hacker aesthetic.

```json
{
  "scene": "code_rain",
  "data": {
    "charset": "katakana|binary|hex|emoji|raccoon",
    "speed": 1.0,
    "density": 0.7,
    "highlight_text": null
  }
}
```

**Charsets:**
- **katakana** — Classic Matrix look
- **binary** — 0s and 1s
- **hex** — 0x deadbeef energy
- **emoji** — Falling emoji (chaotic good)
- **raccoon** — 🦝🗑️✨🌙 themed characters

### 4. `emoji` — Big Emoji Reaction
Full-screen emoji with animation.

```json
{
  "scene": "emoji",
  "data": {
    "emoji": "🦝",
    "animation": "bounce|spin|pulse|explode|float",
    "size": "large",
    "count": 1
  }
}
```

### 5. `text` — Text Display
Speech bubbles, status messages, witty quips.

```json
{
  "scene": "text",
  "data": {
    "text": "I'm not a trash panda, I'm a *selective recycler*",
    "style": "bubble|terminal|handwritten|glitch|typewriter",
    "font_size": "small|medium|large",
    "align": "center"
  }
}
```

### 6. `thinking` — Processing State
Ada is thinking. Show it.

```json
{
  "scene": "thinking",
  "data": {
    "pattern": "galaxy|neural|loading|dots|orbit",
    "progress": null,
    "text": "hmm..."
  }
}
```

**Patterns:**
- **galaxy** — Swirling nebula/galaxy rotation
- **neural** — Branching synaptic connections lighting up
- **loading** — Spinning rings
- **dots** — Classic bouncing dots
- **orbit** — Particles orbiting center point

### 7. `listening` — Audio Input Active
Ada hears you. Reactive waveform.

```json
{
  "scene": "listening",
  "data": {
    "amplitude": 0.5,
    "pattern": "waveform|ripple|bars|ring",
    "sensitivity": 1.0
  }
}
```

### 8. `visualizer` — Audio Output Visualization
Playing TTS audio. Show it visually.

```json
{
  "scene": "visualizer",
  "data": {
    "amplitude": 0.7,
    "pattern": "bars|circle|blob|wave",
    "frequency_bands": [0.2, 0.5, 0.8, 0.3, 0.6]
  }
}
```

## Mood System

Every scene includes a `mood` object that influences colors globally:

```json
{
  "mood": {
    "color_primary": "#00FFCC",
    "color_secondary": "#1a1a2e",
    "intensity": 0.5,
    "pulse_speed": 1.0
  }
}
```

### Mood Presets (set by ada_brain.py)

| Mood | Primary | Secondary | Intensity | Pulse |
|------|---------|-----------|-----------|-------|
| neutral | #00FFCC | #1a1a2e | 0.4 | 0.8 |
| happy | #FFD700 | #1a0a2e | 0.7 | 1.2 |
| excited | #FF6B6B | #2e1a1a | 0.9 | 2.0 |
| sarcastic | #9B59B6 | #1a1a2e | 0.6 | 0.5 |
| curious | #3498DB | #0a1a2e | 0.5 | 1.0 |
| sleepy | #2C3E50 | #0a0a15 | 0.2 | 0.3 |
| chaotic | #FF00FF | #001a00 | 1.0 | 3.0 |
| cozy | #FF8C00 | #2e1a0a | 0.5 | 0.6 |

## Transitions

| Transition | Description |
|-----------|-------------|
| `fade` | Crossfade (default, 300ms) |
| `slide_up` | New scene slides in from bottom |
| `slide_down` | New scene slides in from top |
| `dissolve` | Pixel-by-pixel dissolve |
| `glitch` | Brief digital glitch effect |
| `none` | Instant switch |

## Touch Interactions

The ESP32 sends touch events. The server decides what to do:

| Event | Default Action |
|-------|---------------|
| `tap` | Cycle to next ambient pattern |
| `hold` (2s) | Toggle listening mode |
| `swipe_up` | Show weather |
| `swipe_down` | Show time/status |
| `swipe_left` | Previous scene |
| `swipe_right` | Next scene |
| `double_tap` | Trigger random fun animation |

## Design Philosophy

1. **AMOLED-aware** — Use true black (#000000) backgrounds. Those pixels are OFF. Battery life. Contrast. Beauty.
2. **466×466 is small** — Keep text large, content simple. This isn't a phone.
3. **Ambient first** — The belly should look alive even when nothing is happening.
4. **Whimsy > utility** — A raccoon's belly should be delightful, not a dashboard.
5. **Smooth always** — Target 30fps minimum. Jerky = broken.
6. **Touch is bonus** — Most interaction is voice. Touch is for fidgeting.

## Color Philosophy

Ada's palette is cyberpunk-raccoon:
- **Teals and cyans** (#00FFCC) — Primary identity
- **Deep purples** (#9B59B6) — Sarcasm mode
- **Warm golds** (#FFD700) — Happy/excited
- **Hot pinks** (#FF00FF) — Chaos mode
- **True black** (#000000) — AMOLED beauty
- **Dark navy** (#1a1a2e) — Default background
