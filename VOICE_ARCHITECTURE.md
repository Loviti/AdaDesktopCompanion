# Ada Voice Architecture

## Overview

Real-time voice communication between the ESP32 display and Ada via Unmute/Mochi server.

```
┌─────────────────────────────────────────────────────────────────┐
│                        aiserver (RTX 3060)                      │
│                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐ │
│  │ Kyutai STT  │◄───│   Unmute    │───►│ Ada (OpenClaw LLM)  │ │
│  │ (stt-1b)    │    │   Backend   │    │ via VLLM or direct  │ │
│  └─────────────┘    └──────┬──────┘    └─────────────────────┘ │
│  ┌─────────────┐           │                                    │
│  │ Kyutai TTS  │◄──────────┘                                    │
│  │ (tts-1.6b)  │                                                │
│  └─────────────┘                                                │
└────────────────────────────┬────────────────────────────────────┘
                             │ WebSocket (audio + events)
                             │
              ┌──────────────▼──────────────┐
              │   ESP32-S3 Display          │
              │   ┌─────────────────────┐   │
              │   │  Particle System    │   │
              │   │  (visual feedback)  │   │
              │   └─────────────────────┘   │
              │   ┌─────────┐ ┌─────────┐   │
              │   │ES7210   │ │ES8311   │   │
              │   │Dual Mics│ │Speaker  │   │
              │   └─────────┘ └─────────┘   │
              └─────────────────────────────┘
```

## Components

### 1. ESP32 Audio Firmware
- **Microphone input**: ES7210 dual-mic with echo cancellation
- **Speaker output**: ES8311 codec
- **I2S interface**: 16-bit, 16kHz for STT compatibility
- **WebSocket client**: Streams audio to Unmute backend
- **Visual feedback**: Particles react to voice activity

### 2. Unmute Server
- **Backend**: Python FastAPI WebSocket server
- **STT**: Kyutai STT-1B (already installed at ~/delayed-streams-modeling)
- **TTS**: Kyutai TTS-1.6B (already installed)
- **LLM**: Routes to Ada via OpenClaw

### 3. Ada Integration
Two options:
1. **OpenAI-compatible API**: OpenClaw exposes an API endpoint that Unmute can call
2. **Direct session**: Unmute sends transcripts to a dedicated Ada session

## Implementation Plan

### Phase 1: ESP32 Audio
- [ ] Add ES7210 mic driver to firmware
- [ ] Add ES8311 speaker driver
- [ ] Implement I2S audio capture (16kHz, mono/stereo)
- [ ] Implement I2S audio playback
- [ ] WebSocket audio streaming (Unmute protocol)

### Phase 2: Unmute Server Deployment
- [ ] Clone unmute repo
- [ ] Configure to use local Kyutai STT/TTS
- [ ] Deploy with Docker Compose
- [ ] Test with web frontend first

### Phase 3: Ada LLM Integration
- [ ] Create OpenAI-compatible wrapper for Ada sessions
- [ ] Configure Unmute to use Ada as LLM
- [ ] Add particle state control to responses

### Phase 4: Visual-Audio Sync
- [ ] Particles pulse with incoming audio
- [ ] "Listening" formation when user speaks
- [ ] "Thinking" formation while processing
- [ ] "Talking" formation with TTS playback

## Audio Specifications

### Input (Microphone)
- Codec: ES7210
- Channels: 2 (dual mic for echo cancellation)
- Sample rate: 16000 Hz (STT requirement)
- Bit depth: 16-bit
- Format: PCM

### Output (Speaker)
- Codec: ES8311
- Channels: 1 (mono)
- Sample rate: 24000 Hz (TTS output)
- Bit depth: 16-bit
- Format: PCM

### I2S Pin Configuration (Waveshare ESP32-S3-Touch-AMOLED-1.75)
```cpp
// I2S pins (need to verify from schematic)
#define I2S_BCK     GPIO_NUM_XX  // Bit clock
#define I2S_WS      GPIO_NUM_XX  // Word select
#define I2S_DOUT    GPIO_NUM_XX  // Data out (to ES8311)
#define I2S_DIN     GPIO_NUM_XX  // Data in (from ES7210)

// I2C for codec control
#define CODEC_SDA   15  // Shared I2C bus
#define CODEC_SCL   14
```

## WebSocket Protocol (Unmute-compatible)

### Audio Input (ESP32 → Server)
```json
{
  "type": "input_audio_buffer.append",
  "audio": "<base64-encoded-pcm>"
}
```

### Audio Output (Server → ESP32)
```json
{
  "type": "response.audio.delta",
  "delta": "<base64-encoded-pcm>"
}
```

### State Updates (Server → ESP32)
```json
{
  "type": "ada.state",
  "mood": { "valence": 0.5, "arousal": 0.7 },
  "formation": "thinking"
}
```

## Files to Create

```
AdaDesktopCompanion/
├── firmware/
│   └── src/
│       ├── audio.h          # Audio system header
│       ├── audio.cpp        # ES7210/ES8311 drivers
│       └── voice_client.h   # Unmute WebSocket client
├── server/
│   ├── docker-compose.yml   # Unmute + STT + TTS
│   ├── ada_llm_bridge.py    # OpenClaw → OpenAI API wrapper
│   └── voices.yaml          # Voice configuration
└── VOICE_ARCHITECTURE.md    # This file
```

## Hardware Notes

### ES7210 (Microphone ADC)
- 4-channel audio ADC
- I2C address: 0x40 (default)
- Supports 8kHz to 96kHz
- Built-in ALC, noise gate

### ES8311 (Speaker DAC)
- Low-power mono DAC
- I2C address: 0x18 (default)
- Supports 8kHz to 96kHz
- Built-in headphone amp

### GPIO Expander (TCA9554)
- May control codec power/reset pins
- I2C address: 0x20 or 0x27
