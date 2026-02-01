"""
Ada Desktop Companion - Configuration

All ports, paths, API keys, and tuning parameters.
"""

import os
from pathlib import Path
from dotenv import load_dotenv

load_dotenv()

# =============================================================================
# Network
# =============================================================================

# Ada's WebSocket servers
SCREEN_WS_HOST = "0.0.0.0"
SCREEN_WS_PORT = 8765       # Screen commands → ESP32

AUDIO_WS_HOST = "0.0.0.0"
AUDIO_WS_PORT = 8766        # Audio I/O ↔ clients

# Kyutai DSM (moshi-server)
STT_WS_URL = "ws://localhost:8090"   # Speech-to-text
TTS_WS_URL = "ws://localhost:8089"   # Text-to-speech

# =============================================================================
# OpenAI (LLM)
# =============================================================================

OPENAI_API_KEY = os.getenv("OPENAI_API_KEY", "")
OPENAI_MODEL = os.getenv("OPENAI_MODEL", "gpt-4o-mini")  # fast + cheap
OPENAI_MAX_TOKENS = 1024
OPENAI_TEMPERATURE = 0.8   # Ada is creative but not unhinged

# =============================================================================
# Paths
# =============================================================================

# Ada's memory/identity files (read-only from here)
CLAWD_DIR = Path("/home/chase/clawd")
SOUL_PATH = CLAWD_DIR / "SOUL.md"
USER_PATH = CLAWD_DIR / "USER.md"
MEMORY_PATH = CLAWD_DIR / "MEMORY.md"
IDENTITY_PATH = CLAWD_DIR / "IDENTITY.md"

# Kyutai DSM installation
DSM_DIR = Path("/home/chase/delayed-streams-modeling")

# =============================================================================
# Audio
# =============================================================================

AUDIO_SAMPLE_RATE = 24000   # moshi default
AUDIO_CHANNELS = 1
AUDIO_DTYPE = "int16"       # 16-bit PCM
AUDIO_CHUNK_SIZE = 4800     # 200ms chunks at 24kHz

# =============================================================================
# Screen
# =============================================================================

SCREEN_WIDTH = 466
SCREEN_HEIGHT = 466
SCREEN_FPS = 30
IDLE_SCENE_CYCLE_SEC = 45   # Rotate ambient patterns every 45s
AMBIENT_PATTERNS = [
    "breathing", "sparkles", "aurora", "fireflies", "constellation"
]

# =============================================================================
# Mood Presets
# =============================================================================

MOOD_PRESETS = {
    "neutral": {
        "color_primary": "#00FFCC",
        "color_secondary": "#1a1a2e",
        "intensity": 0.4,
        "pulse_speed": 0.8,
    },
    "happy": {
        "color_primary": "#FFD700",
        "color_secondary": "#1a0a2e",
        "intensity": 0.7,
        "pulse_speed": 1.2,
    },
    "excited": {
        "color_primary": "#FF6B6B",
        "color_secondary": "#2e1a1a",
        "intensity": 0.9,
        "pulse_speed": 2.0,
    },
    "sarcastic": {
        "color_primary": "#9B59B6",
        "color_secondary": "#1a1a2e",
        "intensity": 0.6,
        "pulse_speed": 0.5,
    },
    "curious": {
        "color_primary": "#3498DB",
        "color_secondary": "#0a1a2e",
        "intensity": 0.5,
        "pulse_speed": 1.0,
    },
    "sleepy": {
        "color_primary": "#2C3E50",
        "color_secondary": "#0a0a15",
        "intensity": 0.2,
        "pulse_speed": 0.3,
    },
    "chaotic": {
        "color_primary": "#FF00FF",
        "color_secondary": "#001a00",
        "intensity": 1.0,
        "pulse_speed": 3.0,
    },
    "cozy": {
        "color_primary": "#FF8C00",
        "color_secondary": "#2e1a0a",
        "intensity": 0.5,
        "pulse_speed": 0.6,
    },
}

DEFAULT_MOOD = "neutral"

# =============================================================================
# Image Generation (SD-Turbo)
# =============================================================================

IMAGE_GEN_MODEL = "stabilityai/sd-turbo"
IMAGE_GEN_WIDTH = 128
IMAGE_GEN_HEIGHT = 128
IMAGE_GEN_STEPS = 1       # SD-Turbo does 1-step diffusion
IMAGE_GEN_GUIDANCE = 0.0  # SD-Turbo uses 0 guidance scale

# =============================================================================
# Tools
# =============================================================================

# Weather (OpenWeatherMap)
WEATHER_API_KEY = os.getenv("WEATHER_API_KEY", "")
WEATHER_DEFAULT_LOCATION = "Flint,MI,US"
WEATHER_UNITS = "imperial"  # Fahrenheit for Chase

# Web search (Brave)
BRAVE_API_KEY = os.getenv("BRAVE_API_KEY", "")

# =============================================================================
# Logging
# =============================================================================

LOG_LEVEL = os.getenv("LOG_LEVEL", "INFO")
LOG_FORMAT = "%(asctime)s [%(name)s] %(levelname)s: %(message)s"
