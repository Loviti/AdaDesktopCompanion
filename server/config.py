"""
Ada Desktop Companion - Configuration

All ports, paths, API keys, and tuning parameters.
Adapted for the new unified server architecture.
"""

import os
from pathlib import Path
from dotenv import load_dotenv

# Load .env from backend directory
_backend_dir = Path(__file__).parent
load_dotenv(_backend_dir / ".env")

# =============================================================================
# Network
# =============================================================================

# ESP32 WebSocket (particle protocol)
SCREEN_WS_HOST = "0.0.0.0"
SCREEN_WS_PORT = 8765

# Web GUI HTTP + WebSocket
GUI_HTTP_HOST = "0.0.0.0"
GUI_HTTP_PORT = 8766

# Kyutai DSM (moshi-server) — optional
STT_WS_URL = os.getenv("STT_WS_URL", "ws://localhost:8090")
TTS_WS_URL = os.getenv("TTS_WS_URL", "ws://localhost:8089")

# =============================================================================
# OpenAI (LLM)
# =============================================================================

OPENAI_API_KEY = os.getenv("OPENAI_API_KEY", "")
OPENAI_MODEL = os.getenv("OPENAI_MODEL", "gpt-4o-mini")
OPENAI_MAX_TOKENS = 1024
OPENAI_TEMPERATURE = 0.8

# =============================================================================
# Paths
# =============================================================================

CLAWD_DIR = Path("/home/chase/clawd")
SOUL_PATH = CLAWD_DIR / "SOUL.md"
USER_PATH = CLAWD_DIR / "USER.md"
MEMORY_PATH = CLAWD_DIR / "MEMORY.md"
IDENTITY_PATH = CLAWD_DIR / "IDENTITY.md"

BACKEND_DIR = _backend_dir
GUI_DIR = _backend_dir / "gui"

# =============================================================================
# Screen / Image
# =============================================================================

SCREEN_WIDTH = 466
SCREEN_HEIGHT = 466
IMAGE_GEN_WIDTH = 128
IMAGE_GEN_HEIGHT = 128
IMAGE_GEN_MODEL = "stabilityai/sd-turbo"
IMAGE_GEN_STEPS = 1
IMAGE_GEN_GUIDANCE = 0.0

# Idle scene cycling
IDLE_SCENE_CYCLE_SEC = 45

# =============================================================================
# Rate Limiting
# =============================================================================

# Max mood updates to ESP32 per second
ESP32_MOOD_RATE_LIMIT = 5

# =============================================================================
# Weather
# =============================================================================

WEATHER_API_KEY = os.getenv("WEATHER_API_KEY", "")
WEATHER_DEFAULT_LOCATION = "Flint,MI,US"
WEATHER_UNITS = "imperial"

# =============================================================================
# Web Search (Brave)
# =============================================================================

BRAVE_API_KEY = os.getenv("BRAVE_API_KEY", "")

# =============================================================================
# Mood Presets (for GUI)
# =============================================================================

MOOD_PRESETS = {
    "neutral":   {"valence": 0.0,  "arousal": 0.3, "certainty": 0.5},
    "happy":     {"valence": 0.6,  "arousal": 0.5, "certainty": 0.7},
    "excited":   {"valence": 0.8,  "arousal": 0.9, "certainty": 0.8},
    "sarcastic": {"valence": -0.2, "arousal": 0.4, "certainty": 0.9},
    "curious":   {"valence": 0.2,  "arousal": 0.6, "certainty": 0.3},
    "sleepy":    {"valence": 0.1,  "arousal": 0.1, "certainty": 0.4},
    "chaotic":   {"valence": 0.3,  "arousal": 1.0, "certainty": 0.2},
    "cozy":      {"valence": 0.5,  "arousal": 0.2, "certainty": 0.6},
}

DEFAULT_MOOD = "neutral"

# =============================================================================
# Logging
# =============================================================================

LOG_LEVEL = os.getenv("LOG_LEVEL", "INFO")
LOG_FORMAT = "%(asctime)s [%(name)s] %(levelname)s: %(message)s"
