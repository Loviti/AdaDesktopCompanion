"""
Ada Desktop Companion - Screen Engine

Generates scene commands for the ESP32 belly display.
Handles ambient idle cycles, contextual scenes, and mood-driven content.

This is what makes Ada's belly *alive*.
"""

import asyncio
import json
import logging
import random
import time
from typing import Optional

import config

logger = logging.getLogger("ada.screen")


class ScreenEngine:
    """Generates and manages display content for Ada's belly screen."""

    def __init__(self):
        self.current_scene: str = "ambient"
        self.current_mood: str = config.DEFAULT_MOOD
        self.mood_data: dict = dict(config.MOOD_PRESETS[config.DEFAULT_MOOD])
        self.is_idle: bool = True
        self._last_ambient_change: float = 0.0
        self._ambient_index: int = 0
        self._subscribers: set = set()  # WebSocket connections
        self._lock = asyncio.Lock()

        # Fun startup messages
        self._startup_quips = [
            "belly screen online 🦝",
            "*stretches* ... ok I'm awake",
            "// TODO: take over the world",
            "raccoon.exe loaded",
            "garbage day is MY day",
            "vibing at 466×466",
            "AMOLED? more like A-MOLE-D (raccoon pun)",
        ]

        # Idle text rotation
        self._idle_texts = [
            "don't mind me, just vibing",
            "I'm not sleeping, I'm optimizing",
            "*raccoon noises*",
            "pet the belly for luck",
            "[ insert wisdom here ]",
            "sudo make me a sandwich",
            "404: productiveness not found",
            "powered by trash and good intentions",
            "I contain multitudes (and snacks)",
        ]

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
        """Send a scene command to all connected screens."""
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
    # Scene Builders
    # =========================================================================

    def _build_scene(
        self,
        scene: str,
        data: dict,
        mood: Optional[str] = None,
        transition: str = "fade",
        duration_ms: int = 300,
    ) -> dict:
        """Build a scene command message."""
        mood_name = mood or self.current_mood
        mood_data = config.MOOD_PRESETS.get(mood_name, self.mood_data)
        return {
            "type": "scene",
            "scene": scene,
            "data": data,
            "mood": mood_data,
            "transition": transition,
            "duration_ms": duration_ms,
        }

    async def send_scene(
        self,
        scene: str,
        data: dict,
        mood: Optional[str] = None,
        transition: str = "fade",
        duration_ms: int = 300,
    ):
        """Build and broadcast a scene to all screens."""
        async with self._lock:
            self.current_scene = scene
            if mood:
                self.set_mood(mood)
            msg = self._build_scene(scene, data, mood, transition, duration_ms)
            await self._broadcast(msg)
            logger.debug(f"Scene: {scene} | Mood: {mood or self.current_mood}")

    # =========================================================================
    # Mood
    # =========================================================================

    def set_mood(self, mood: str):
        """Update the current mood."""
        if mood in config.MOOD_PRESETS:
            self.current_mood = mood
            self.mood_data = dict(config.MOOD_PRESETS[mood])
            logger.debug(f"Mood → {mood}")
        else:
            logger.warning(f"Unknown mood: {mood}, keeping {self.current_mood}")

    # =========================================================================
    # High-Level Scene Methods
    # =========================================================================

    async def show_startup(self):
        """Boot-up animation."""
        quip = random.choice(self._startup_quips)
        await self.send_scene(
            "text",
            {"text": quip, "style": "terminal", "font_size": "medium", "align": "center"},
            mood="curious",
            transition="glitch",
            duration_ms=500,
        )
        await asyncio.sleep(3)
        await self.start_ambient()

    async def start_ambient(self):
        """Begin ambient idle cycle."""
        self.is_idle = True
        self._last_ambient_change = time.time()
        pattern = config.AMBIENT_PATTERNS[self._ambient_index]
        await self.send_scene(
            "ambient",
            {"pattern": pattern, "speed": 1.0, "density": 0.5},
            mood="neutral",
            transition="fade",
            duration_ms=800,
        )

    async def cycle_ambient(self):
        """Rotate to next ambient pattern."""
        self._ambient_index = (self._ambient_index + 1) % len(config.AMBIENT_PATTERNS)
        self._last_ambient_change = time.time()
        pattern = config.AMBIENT_PATTERNS[self._ambient_index]
        await self.send_scene(
            "ambient",
            {"pattern": pattern, "speed": 1.0, "density": 0.5},
            transition="dissolve",
            duration_ms=600,
        )

    async def show_listening(self, amplitude: float = 0.5):
        """Listening state — audio input active."""
        self.is_idle = False
        await self.send_scene(
            "listening",
            {"amplitude": amplitude, "pattern": "waveform", "sensitivity": 1.0},
            mood="curious",
            transition="fade",
            duration_ms=200,
        )

    async def update_listening_amplitude(self, amplitude: float):
        """Update listening visualization amplitude without full scene change."""
        msg = {
            "type": "update",
            "data": {"amplitude": min(1.0, max(0.0, amplitude))},
        }
        await self._broadcast(msg)

    async def show_thinking(self, text: str = "hmm..."):
        """Thinking state — LLM is processing."""
        self.is_idle = False
        patterns = ["galaxy", "neural", "loading", "orbit"]
        await self.send_scene(
            "thinking",
            {"pattern": random.choice(patterns), "progress": None, "text": text},
            mood="curious",
            transition="fade",
            duration_ms=200,
        )

    async def show_talking(self, amplitude: float = 0.5):
        """Talking state — TTS playing."""
        self.is_idle = False
        await self.send_scene(
            "visualizer",
            {
                "amplitude": amplitude,
                "pattern": "circle",
                "frequency_bands": [0.3, 0.5, 0.8, 0.4, 0.6],
            },
            transition="fade",
            duration_ms=150,
        )

    async def update_talk_amplitude(self, amplitude: float):
        """Update visualizer amplitude during speech."""
        msg = {
            "type": "update",
            "data": {"amplitude": min(1.0, max(0.0, amplitude))},
        }
        await self._broadcast(msg)

    async def show_weather(self, weather_data: dict):
        """Display weather widget."""
        self.is_idle = False
        condition = weather_data.get("condition", "cloudy")
        mood_map = {
            "sunny": "happy",
            "rain": "cozy",
            "snow": "cozy",
            "storm": "chaotic",
            "cloudy": "neutral",
            "fog": "sleepy",
            "wind": "curious",
        }
        await self.send_scene(
            "weather",
            weather_data,
            mood=mood_map.get(condition, "neutral"),
            transition="slide_up",
            duration_ms=400,
        )

    async def show_emoji(self, emoji: str, animation: str = "bounce"):
        """Full-screen emoji reaction."""
        was_idle = self.is_idle
        self.is_idle = False
        await self.send_scene(
            "emoji",
            {"emoji": emoji, "animation": animation, "size": "large", "count": 1},
            transition="none",
            duration_ms=0,
        )
        # Auto-return to ambient after 3 seconds
        await asyncio.sleep(3)
        if not self.is_idle and was_idle:
            await self.start_ambient()

    async def show_text(
        self, text: str, style: str = "bubble", mood: Optional[str] = None
    ):
        """Display text on belly."""
        self.is_idle = False
        # Pick font size based on text length
        if len(text) < 20:
            font_size = "large"
        elif len(text) < 60:
            font_size = "medium"
        else:
            font_size = "small"

        await self.send_scene(
            "text",
            {"text": text, "style": style, "font_size": font_size, "align": "center"},
            mood=mood,
            transition="fade",
            duration_ms=300,
        )

    async def show_code_rain(self, charset: str = "katakana"):
        """Matrix-style code rain. Because raccoon."""
        self.is_idle = False
        await self.send_scene(
            "code_rain",
            {"charset": charset, "speed": 1.0, "density": 0.7, "highlight_text": None},
            mood="chaotic",
            transition="glitch",
            duration_ms=200,
        )

    async def show_idle_text(self):
        """Show a random idle quip, then return to ambient."""
        text = random.choice(self._idle_texts)
        await self.show_text(text, style="handwritten", mood="neutral")
        await asyncio.sleep(5)
        await self.start_ambient()

    async def show_reaction(self, mood: str, text: Optional[str] = None):
        """Quick reaction based on mood — emoji + optional text."""
        emoji_map = {
            "happy": "😊",
            "excited": "🎉",
            "sarcastic": "😏",
            "curious": "🤔",
            "chaotic": "😈",
            "cozy": "☕",
            "sleepy": "😴",
            "neutral": "🦝",
        }
        emoji = emoji_map.get(mood, "🦝")
        await self.show_emoji(emoji, animation="pulse")
        if text:
            await asyncio.sleep(1.5)
            await self.show_text(text, mood=mood)

    # =========================================================================
    # Touch Handling
    # =========================================================================

    async def handle_touch(self, event: dict) -> Optional[str]:
        """Process touch events from the ESP32. Returns action taken."""
        touch_type = event.get("event", "tap")

        if touch_type == "tap":
            if self.is_idle:
                await self.cycle_ambient()
                return "cycled_ambient"
            return None

        elif touch_type == "hold":
            return "toggle_listening"  # Server handles this

        elif touch_type == "swipe_up":
            return "show_weather"  # Server fetches weather

        elif touch_type == "swipe_down":
            await self.show_text(
                time.strftime("%I:%M %p"),
                style="terminal",
                mood="neutral",
            )
            return "show_time"

        elif touch_type == "double_tap":
            # Random fun thing
            fun_actions = [
                lambda: self.show_code_rain(random.choice(["katakana", "emoji", "raccoon"])),
                lambda: self.show_emoji("🦝", "spin"),
                lambda: self.show_emoji("✨", "explode"),
                lambda: self.show_idle_text(),
                lambda: self.show_text("boop!", style="glitch", mood="chaotic"),
            ]
            await random.choice(fun_actions)()
            return "fun_action"

        return None

    # =========================================================================
    # Idle Loop
    # =========================================================================

    async def idle_loop(self):
        """Background loop that cycles ambient scenes when idle."""
        logger.info("Idle loop started")
        while True:
            try:
                await asyncio.sleep(5)  # Check every 5s
                if not self.is_idle:
                    continue
                elapsed = time.time() - self._last_ambient_change
                if elapsed >= config.IDLE_SCENE_CYCLE_SEC:
                    # Occasionally show a text quip instead of pattern
                    if random.random() < 0.15:
                        await self.show_idle_text()
                    else:
                        await self.cycle_ambient()
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Idle loop error: {e}")
                await asyncio.sleep(10)
