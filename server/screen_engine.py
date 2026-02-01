"""
Ada Desktop Companion - Screen Engine (Particle System)

Generates particle system commands for the ESP32 belly display.
Images are decomposed into colored particles that float, swirl, and pulse
according to Ada's emotional state.

This is what makes Ada's belly *alive*.
"""

import asyncio
import base64
import json
import logging
import random
import time
from typing import Optional

import config

logger = logging.getLogger("ada.screen")


def _lerp(a: float, b: float, t: float) -> float:
    """Linear interpolation."""
    return a + (b - a) * max(0.0, min(1.0, t))


class ScreenEngine:
    """Generates and manages particle display content for Ada's belly screen."""

    def __init__(self):
        # Emotional state (updated by brain/voice pipeline)
        self.valence: float = 0.0       # -1 (negative) to 1 (positive)
        self.arousal: float = 0.3       # 0 (calm) to 1 (excited)
        self.certainty: float = 0.5     # 0 (uncertain) to 1 (certain)
        self.mode: str = "IDLE"         # IDLE, LISTENING, THINKING, TALKING

        # Current image (raw RGB bytes, cached for mood-only updates)
        self._current_image: Optional[bytes] = None
        self._current_image_width: int = config.IMAGE_GEN_WIDTH
        self._current_image_height: int = config.IMAGE_GEN_HEIGHT

        self.is_idle: bool = True
        self._subscribers: set = set()
        self._lock = asyncio.Lock()

        # Idle cycle
        self._last_idle_change: float = 0.0
        self._idle_prompts = [
            "soft glowing raccoon silhouette dark background",
            "gentle blue green nebula particles",
            "warm amber fireflies night forest",
            "aurora borealis green cyan ribbons",
            "constellation stars dark sky faint lines",
            "calm ocean waves moonlight reflection",
            "floating lanterns warm orange night",
            "crystal cave blue purple glow",
        ]
        self._idle_index: int = 0

    # =========================================================================
    # Subscriber Management
    # =========================================================================

    def subscribe(self, ws):
        """Register a screen WebSocket connection."""
        self._subscribers.add(ws)
        logger.info(f"Screen client connected. Total: {len(self._subscribers)}")

    def unsubscribe(self, ws):
        """Remove a screen WebSocket connection."""
        self._subscribers.discard(ws)
        logger.info(f"Screen client disconnected. Total: {len(self._subscribers)}")

    async def _broadcast(self, message: dict):
        """Send a message to all connected screens."""
        if not self._subscribers:
            return
        payload = json.dumps(message)
        dead = set()
        for ws in self._subscribers:
            try:
                await ws.send(payload)
            except Exception:
                dead.add(ws)
        for ws in dead:
            self._subscribers.discard(ws)

    # =========================================================================
    # Particle Config from Emotional State
    # =========================================================================

    def _build_particle_config(self) -> dict:
        """
        Map emotional state to particle physics parameters.

        arousal (0-1): energy level
        valence (-1 to 1): mood positivity
        certainty (0-1): confidence/clarity
        mode: behavioral animation type
        """
        # Arousal → energy
        particle_speed = _lerp(0.3, 3.0, self.arousal)
        dispersion = _lerp(10.0, 100.0, self.arousal)
        pulse_speed = _lerp(0.5, 3.0, self.arousal)

        # Valence → warmth/size
        # Map from [-1, 1] to [0, 1] for lerp
        valence_norm = (self.valence + 1.0) / 2.0
        particle_size = _lerp(1.5, 5.0, valence_norm)

        # Certainty → density/connections
        particle_count = int(_lerp(300, 1500, self.certainty))
        link_count = int(_lerp(0, 80, self.certainty))
        opacity = _lerp(0.5, 1.0, self.certainty)

        # Mode → animation type and overrides
        animation = "float"
        rotation_speed = 0.0

        if self.mode == "THINKING":
            animation = "swirl_inward"
            rotation_speed = 2.0
            dispersion = _lerp(10.0, 40.0, self.arousal)  # Tighter swirl
            particle_speed = max(particle_speed, 1.0)
        elif self.mode == "TALKING":
            animation = "pulse_outward"
            particle_speed = max(particle_speed, 1.5)
            pulse_speed = max(pulse_speed, 1.5)
        elif self.mode == "LISTENING":
            animation = "float"
            particle_speed = min(particle_speed, 0.8)
            dispersion = min(dispersion, 30.0)
        elif self.mode == "IDLE":
            animation = "drift"
            particle_speed = min(particle_speed, 0.5)
            pulse_speed = min(pulse_speed, 0.8)

        return {
            "particle_count": particle_count,
            "particle_size": round(particle_size, 1),
            "particle_speed": round(particle_speed, 2),
            "dispersion": round(dispersion, 1),
            "opacity": round(opacity, 2),
            "shape": "circle",
            "animation": animation,
            "link_count": link_count,
            "link_opacity": 0.2,
            "bg_color": "#000000",
            "color_mode": "original",
            "pulse_speed": round(pulse_speed, 2),
            "rotation_speed": round(rotation_speed, 2),
        }

    # =========================================================================
    # High-Level Commands
    # =========================================================================

    async def send_particles(self, image_data: bytes, width: int, height: int):
        """
        Send a new image as particles to the display.

        Args:
            image_data: Raw RGB pixel bytes (width * height * 3)
            width: Image width
            height: Image height
        """
        async with self._lock:
            self._current_image = image_data
            self._current_image_width = width
            self._current_image_height = height
            self.is_idle = False

            image_b64 = base64.b64encode(image_data).decode("ascii")

            message = {
                "type": "particles",
                "image": image_b64,
                "width": width,
                "height": height,
                "config": self._build_particle_config(),
            }

            await self._broadcast(message)
            logger.info(
                f"Sent particle image: {width}x{height}, "
                f"mode={self.mode}, arousal={self.arousal:.2f}, "
                f"valence={self.valence:.2f}"
            )

    async def send_mood_update(self):
        """
        Send a mood-only update (no new image) — changes particle physics.
        """
        message = {
            "type": "mood",
            "config": self._build_particle_config(),
        }
        await self._broadcast(message)
        logger.debug(
            f"Mood update: mode={self.mode}, arousal={self.arousal:.2f}, "
            f"valence={self.valence:.2f}, certainty={self.certainty:.2f}"
        )

    async def send_clear(self):
        """Send clear command — fade particles out."""
        message = {"type": "clear"}
        await self._broadcast(message)
        logger.debug("Sent clear")

    # =========================================================================
    # State Setters
    # =========================================================================

    def set_emotional_state(
        self,
        valence: Optional[float] = None,
        arousal: Optional[float] = None,
        certainty: Optional[float] = None,
        mode: Optional[str] = None,
    ):
        """Update emotional state values."""
        if valence is not None:
            self.valence = max(-1.0, min(1.0, valence))
        if arousal is not None:
            self.arousal = max(0.0, min(1.0, arousal))
        if certainty is not None:
            self.certainty = max(0.0, min(1.0, certainty))
        if mode is not None:
            self.mode = mode

    def set_mood(self, mood: str):
        """
        Map named moods to emotional state values.
        Backward-compatible with the old mood system.
        """
        mood_map = {
            "neutral":  {"valence": 0.0,  "arousal": 0.3, "certainty": 0.5},
            "happy":    {"valence": 0.6,  "arousal": 0.5, "certainty": 0.7},
            "excited":  {"valence": 0.8,  "arousal": 0.9, "certainty": 0.8},
            "sarcastic": {"valence": -0.2, "arousal": 0.4, "certainty": 0.9},
            "curious":  {"valence": 0.2,  "arousal": 0.6, "certainty": 0.3},
            "sleepy":   {"valence": 0.1,  "arousal": 0.1, "certainty": 0.4},
            "chaotic":  {"valence": 0.3,  "arousal": 1.0, "certainty": 0.2},
            "cozy":     {"valence": 0.5,  "arousal": 0.2, "certainty": 0.6},
        }

        state = mood_map.get(mood, mood_map["neutral"])
        self.set_emotional_state(**state)
        logger.debug(f"Mood → {mood}: {state}")

    # =========================================================================
    # Mode Transitions (called by voice pipeline)
    # =========================================================================

    async def show_listening(self, amplitude: float = 0.5):
        """Transition to listening state."""
        self.is_idle = False
        self.set_emotional_state(mode="LISTENING", arousal=0.3)
        await self.send_mood_update()

    async def show_thinking(self, text: str = ""):
        """Transition to thinking state."""
        self.is_idle = False
        self.set_emotional_state(mode="THINKING", arousal=0.6)
        await self.send_mood_update()

    async def show_talking(self, amplitude: float = 0.5):
        """Transition to talking state."""
        self.is_idle = False
        self.set_emotional_state(mode="TALKING", arousal=0.5 + amplitude * 0.4)
        await self.send_mood_update()

    async def update_talk_amplitude(self, amplitude: float):
        """Update arousal from speech amplitude during talking."""
        self.arousal = max(0.3, min(1.0, 0.4 + amplitude * 0.6))
        await self.send_mood_update()

    async def update_listening_amplitude(self, amplitude: float):
        """Update arousal from mic amplitude during listening."""
        self.arousal = max(0.1, min(0.6, 0.2 + amplitude * 0.4))
        await self.send_mood_update()

    async def start_ambient(self):
        """Return to idle ambient state with gentle particles."""
        self.is_idle = True
        self.set_emotional_state(mode="IDLE", arousal=0.2, valence=0.1, certainty=0.5)
        self._last_idle_change = time.time()
        await self.send_mood_update()

    # =========================================================================
    # Image Display (called by tool executor / brain)
    # =========================================================================

    async def show_generated_image(self, image_data: bytes, width: int, height: int):
        """
        Display a generated image as particles.
        Called when Ada uses the generate_visual tool.
        """
        self.is_idle = False
        await self.send_particles(image_data, width, height)

    # =========================================================================
    # Backward-Compatible Methods
    # =========================================================================

    async def show_startup(self):
        """Startup sequence — send a mood-only message with gentle emergence."""
        self.set_emotional_state(mode="IDLE", arousal=0.1, valence=0.3, certainty=0.3)
        # Just send a mood update; the ESP32 plays its own startup animation
        await self.send_mood_update()
        logger.info("Startup sequence sent")

    async def show_emoji(self, emoji: str, animation: str = "bounce"):
        """Legacy emoji support — send as mood update for now."""
        # Map common emoji to emotional states
        emoji_moods = {
            "😊": ("happy", 0.5, 0.6),
            "🎉": ("excited", 0.9, 0.8),
            "😏": ("sarcastic", 0.4, -0.2),
            "🤔": ("curious", 0.6, 0.2),
            "😈": ("chaotic", 1.0, 0.3),
            "☕": ("cozy", 0.2, 0.5),
            "😴": ("sleepy", 0.1, 0.1),
            "🦝": ("neutral", 0.4, 0.3),
        }
        mood, arousal, valence = emoji_moods.get(emoji, ("neutral", 0.4, 0.0))
        self.set_mood(mood)
        await self.send_mood_update()

    async def show_text(self, text: str, style: str = "bubble", mood: Optional[str] = None):
        """Legacy text display — update mood, text display handled by future screen overlay."""
        if mood:
            self.set_mood(mood)
        await self.send_mood_update()

    async def show_reaction(self, mood: str, text: Optional[str] = None):
        """Legacy reaction — map to mood update."""
        self.set_mood(mood)
        await self.send_mood_update()

    async def show_weather(self, weather_data: dict):
        """Legacy weather display — update mood based on weather."""
        condition = weather_data.get("condition", "cloudy")
        mood_map = {
            "sunny": "happy", "rain": "cozy", "snow": "cozy",
            "storm": "chaotic", "cloudy": "neutral", "fog": "sleepy",
        }
        self.set_mood(mood_map.get(condition, "neutral"))
        await self.send_mood_update()

    async def show_code_rain(self, charset: str = "katakana"):
        """Legacy code rain — set chaotic mood."""
        self.set_mood("chaotic")
        self.set_emotional_state(mode="IDLE", arousal=0.8)
        await self.send_mood_update()

    async def cycle_ambient(self):
        """Cycle to next idle visual."""
        self._idle_index = (self._idle_index + 1) % len(self._idle_prompts)
        self._last_idle_change = time.time()
        # Just update mood; actual image generation happens in idle_loop if image_gen is available
        await self.send_mood_update()

    async def handle_touch(self, event: dict) -> Optional[str]:
        """Process touch events from the ESP32."""
        touch_type = event.get("event", "tap")

        if touch_type == "tap":
            if self.is_idle:
                await self.cycle_ambient()
                return "cycled_ambient"
            return None
        elif touch_type == "hold":
            return "toggle_listening"
        elif touch_type == "swipe_up":
            return "show_weather"
        elif touch_type == "double_tap":
            self.set_mood("chaotic")
            self.set_emotional_state(arousal=1.0)
            await self.send_mood_update()
            return "fun_action"

        return None

    # =========================================================================
    # Idle Loop
    # =========================================================================

    async def idle_loop(self):
        """Background loop that manages idle state and generates ambient visuals."""
        logger.info("Idle loop started")

        while True:
            try:
                await asyncio.sleep(5)

                if not self.is_idle:
                    continue

                elapsed = time.time() - self._last_idle_change
                if elapsed >= config.IDLE_SCENE_CYCLE_SEC:
                    self._last_idle_change = time.time()

                    # Try to generate an ambient image if image_gen is available
                    try:
                        import image_gen
                        if await image_gen.is_ready():
                            prompt = self._idle_prompts[self._idle_index]
                            self._idle_index = (self._idle_index + 1) % len(self._idle_prompts)

                            rgb_data = await image_gen.generate_image(prompt)
                            if rgb_data:
                                self.set_emotional_state(mode="IDLE", arousal=0.2)
                                await self.send_particles(
                                    rgb_data,
                                    config.IMAGE_GEN_WIDTH,
                                    config.IMAGE_GEN_HEIGHT,
                                )
                                continue
                    except ImportError:
                        pass
                    except Exception as e:
                        logger.warning(f"Idle image gen failed: {e}")

                    # Fallback: just cycle mood
                    await self.cycle_ambient()

            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Idle loop error: {e}")
                await asyncio.sleep(10)
