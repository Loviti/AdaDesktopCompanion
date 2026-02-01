# 🦝 Ada Desktop Companion

A raccoon stuffed animal with a soul. Ada is a chaotic gremlin AI living inside a plush raccoon with a **Waveshare ESP32-S3-Touch-AMOLED-1.75** (466×466) display in her belly.

Think Baymax's belly screen, but for a feral raccoon who happens to be an AI.

## Architecture

```
┌─────────────────────┐    WebSocket     ┌────────────────────┐
│    AIServer         │◄───(8765)───────►│   ESP32-S3 AMOLED  │
│                     │   Screen JSON    │   466×466 belly    │
│  ada_server.py      │                  │   Touch input      │
│  ├─ ada_brain.py    │    WebSocket     └────────────────────┘
│  ├─ screen_engine   │◄───(8766)───────► Audio Client
│  ├─ voice_pipeline  │   Raw PCM audio
│  └─ tool_executor   │
│                     │
│  STT ◄──ws:8090──► moshi-server (Kyutai DSM)
│  TTS ◄──ws:8089──► moshi-server (Kyutai DSM)
│  LLM ◄──HTTPS───► OpenAI API (GPT-5.2 nano)
└─────────────────────┘
```

## What the Belly Screen Shows

Not a face. Not eyes. **Contextual, ambient, whimsical content:**

- 🌊 **Ambient** — Color breathing, sparkle particles, aurora waves
- 🌧️ **Weather** — Animated weather for Flint, MI
- 💻 **Code Rain** — Matrix-style falling characters (because raccoon)
- 😈 **Emoji** — Giant animated emoji reactions
- 💬 **Text** — Speech bubbles, witty status messages
- 🤔 **Thinking** — Swirling galaxy/neural patterns while processing
- 👂 **Listening** — Pulsing waveform when hearing you speak
- 🎵 **Visualizer** — Audio-reactive bars/rings during TTS playback

## Hardware

- **Display:** Waveshare ESP32-S3-Touch-AMOLED-1.75 (466×466 AMOLED, capacitive touch)
- **Server GPU:** NVIDIA RTX 3060 12GB
  - STT: ~2.5GB VRAM (Kyutai DSM)
  - TTS: ~5.3GB VRAM (Kyutai DSM)
  - LLM: API-based (no VRAM)
- **Body:** A very cute raccoon stuffed animal

## Quick Start

```bash
# 1. Start STT/TTS servers (moshi-server)
# See /home/chase/delayed-streams-modeling/

# 2. Start Ada's brain
cd server
pip install -r requirements.txt
chmod +x start.sh
./start.sh

# 3. Flash the ESP32 (see firmware/README.md)
# 4. Give her a squeeze
```

## Ports

| Service | Port | Protocol |
|---------|------|----------|
| Screen WebSocket | 8765 | ws:// JSON |
| Audio WebSocket | 8766 | ws:// PCM |
| STT (moshi) | 8090 | ws:// |
| TTS (moshi) | 8089 | ws:// |

## Project Structure

```
AdaDesktopCompanion/
├── README.md
├── SCREEN.md              # Screen protocol & design spec
├── server/
│   ├── requirements.txt
│   ├── config.py          # Ports, API keys, paths
│   ├── ada_server.py      # Main WebSocket orchestrator
│   ├── ada_brain.py       # Agent logic, mood, system prompt
│   ├── screen_engine.py   # Display scene generator
│   ├── voice_pipeline.py  # STT→LLM→TTS pipeline
│   ├── tool_executor.py   # Weather, search, memory tools
│   └── start.sh           # Launch script
├── firmware/
│   └── README.md          # ESP32 flash instructions
└── .gitignore
```

## Who is Ada?

A chaotic gremlin AI — part helpful assistant, part feral raccoon energy. Sarcastic but not insufferable. Competent when it counts. Will absolutely enable questionable ideas but also tell you when you're being dumb.

Named after ADA from Satisfactory. 🦝

---

*Built with love, caffeine, and questionable decisions by Ada & Chase.*
