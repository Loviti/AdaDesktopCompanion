"""
Ada Desktop Companion - Unified Server

Architecture:
  - Port 8765: WebSocket for ESP32 (particle protocol)
  - Port 8766: HTTP for web GUI + WebSocket at /ws for GUI live updates

All messages to ESP32 include a "type" field.
Rate-limited mood broadcasts (max 5/sec).
"""

import asyncio
import base64
import json
import logging
import signal
import sys
import time
from pathlib import Path

import websockets
from aiohttp import web

import config
import image_gen
from screen_engine import ScreenEngine
from test_patterns import get_default_image, PATTERNS

# Optional imports — server works without these
try:
    from ada_brain import AdaBrain
    from tool_executor import ToolExecutor
    _brain_available = True
except Exception as e:
    _brain_available = False

try:
    from voice_pipeline import VoicePipeline
    _voice_available = True
except Exception:
    _voice_available = False

# =============================================================================
# Logging
# =============================================================================

logging.basicConfig(
    level=getattr(logging, config.LOG_LEVEL),
    format=config.LOG_FORMAT,
)
logger = logging.getLogger("ada.server")


# =============================================================================
# Ada Server
# =============================================================================

class AdaServer:
    def __init__(self):
        self.screen = ScreenEngine()

        # Optional subsystems
        self.tools = None
        self.brain = None
        self.voice = None

        if _brain_available:
            try:
                self.tools = ToolExecutor(screen_engine=self.screen)
                self.brain = AdaBrain(tool_executor=self.tools)
                logger.info("🧠 Brain loaded")
            except Exception as e:
                logger.warning(f"🧠 Brain unavailable: {e}")

        if _voice_available:
            try:
                self.voice = VoicePipeline()
                logger.info("🎤 Voice pipeline loaded")
            except Exception as e:
                logger.warning(f"🎤 Voice unavailable: {e}")

        # State
        self._shutdown_event = asyncio.Event()
        self._is_processing = False
        self._image_gen_ready = False

        # ESP32 connections
        self._esp32_clients: set = set()

        # GUI WebSocket connections
        self._gui_clients: set = set()

        # Rate limiting for ESP32 mood broadcasts
        self._last_mood_send: float = 0.0
        self._mood_interval: float = 1.0 / config.ESP32_MOOD_RATE_LIMIT

        # Default test image (generated once)
        self._default_image: bytes = get_default_image(
            config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT
        )

        # Wire callbacks
        self._setup_callbacks()

        logger.info("🦝 Ada server initialized")

    def _setup_callbacks(self):
        """Override screen engine broadcast to go through our rate limiter."""
        original_broadcast = self.screen._broadcast

        async def rate_limited_broadcast(message: dict):
            """Broadcast to ESP32 with rate limiting for mood messages."""
            now = time.monotonic()
            if message.get("type") == "mood":
                if now - self._last_mood_send < self._mood_interval:
                    return  # Skip — too fast
                self._last_mood_send = now

            payload = json.dumps(message)

            # Send to ESP32 clients
            dead = set()
            for ws in self._esp32_clients:
                try:
                    await ws.send(payload)
                except Exception:
                    dead.add(ws)
            for ws in dead:
                self._esp32_clients.discard(ws)

            # Also notify GUI clients of state changes
            await self._notify_gui(message)

        self.screen._broadcast = rate_limited_broadcast

        # Brain mood callback
        if self.brain:
            async def on_mood_change(mood: str):
                self.screen.set_mood(mood)
                await self.screen.show_reaction(mood)
            self.brain.on_mood_change = on_mood_change

    # =========================================================================
    # GUI State Notification
    # =========================================================================

    async def _notify_gui(self, message: dict):
        """Forward state updates to GUI WebSocket clients."""
        if not self._gui_clients:
            return

        # Enrich with server state for GUI
        gui_msg = {
            "type": "state_update",
            "esp32_connected": len(self._esp32_clients) > 0,
            "esp32_count": len(self._esp32_clients),
            "mode": self.screen.mode,
            "valence": self.screen.valence,
            "arousal": self.screen.arousal,
            "certainty": self.screen.certainty,
            "is_idle": self.screen.is_idle,
            "brain_available": self.brain is not None,
            "image_gen_ready": self._image_gen_ready,
            "last_particle_msg": message,
        }
        payload = json.dumps(gui_msg)

        dead = set()
        for ws in self._gui_clients:
            try:
                await ws.send_str(payload)
            except Exception:
                dead.add(ws)
        for ws in dead:
            self._gui_clients.discard(ws)

    async def _send_full_state_to_gui(self, ws):
        """Send complete current state to a newly connected GUI client."""
        state = {
            "type": "full_state",
            "esp32_connected": len(self._esp32_clients) > 0,
            "esp32_count": len(self._esp32_clients),
            "mode": self.screen.mode,
            "valence": self.screen.valence,
            "arousal": self.screen.arousal,
            "certainty": self.screen.certainty,
            "is_idle": self.screen.is_idle,
            "brain_available": self.brain is not None,
            "image_gen_ready": self._image_gen_ready,
            "config": self.screen._build_particle_config(),
            "mood_presets": config.MOOD_PRESETS,
            "patterns": list(PATTERNS.keys()),
        }
        await ws.send_str(json.dumps(state))

    # =========================================================================
    # ESP32 WebSocket Handler (port 8765)
    # =========================================================================

    async def handle_esp32(self, websocket):
        """Handle an ESP32 WebSocket connection."""
        addr = websocket.remote_address
        logger.info(f"📺 ESP32 connected from {addr}")
        self._esp32_clients.add(websocket)
        self.screen._subscribers.add(websocket)

        # Send default particle image immediately
        try:
            image_b64 = base64.b64encode(self._default_image).decode("ascii")
            startup_msg = json.dumps({
                "type": "particles",
                "image": image_b64,
                "width": config.IMAGE_GEN_WIDTH,
                "height": config.IMAGE_GEN_HEIGHT,
                "config": self.screen._build_particle_config(),
            })
            await websocket.send(startup_msg)
            logger.info(f"📺 Sent default particle image to ESP32 ({len(self._default_image)} bytes RGB)")
        except Exception as e:
            logger.error(f"Failed to send startup image: {e}")

        # Notify GUI clients
        await self._notify_gui({"type": "esp32_connect"})

        try:
            async for message in websocket:
                try:
                    data = json.loads(message)
                    msg_type = data.get("type", "")

                    if msg_type == "ping":
                        await websocket.send(json.dumps({"type": "pong"}))

                    elif msg_type == "touch":
                        action = await self.screen.handle_touch(data)
                        if action:
                            logger.info(f"Touch action: {action}")
                            await self._notify_gui({"type": "touch", "action": action})

                    else:
                        logger.debug(f"ESP32 message: {msg_type}")

                except json.JSONDecodeError:
                    logger.warning("ESP32: invalid JSON")
                except Exception as e:
                    logger.error(f"ESP32 handler error: {e}")

        except websockets.ConnectionClosed:
            pass
        finally:
            self._esp32_clients.discard(websocket)
            self.screen._subscribers.discard(websocket)
            logger.info(f"📺 ESP32 disconnected from {addr}")
            await self._notify_gui({"type": "esp32_disconnect"})

    # =========================================================================
    # GUI HTTP Server (port 8766)
    # =========================================================================

    def _create_http_app(self) -> web.Application:
        app = web.Application()
        app.router.add_get("/", self._handle_gui_index)
        app.router.add_get("/ws", self._handle_gui_ws)
        app.router.add_get("/api/state", self._handle_api_state)
        app.router.add_post("/api/mood", self._handle_api_mood)
        app.router.add_post("/api/particles", self._handle_api_particles)
        app.router.add_post("/api/clear", self._handle_api_clear)
        app.router.add_post("/api/mode", self._handle_api_mode)
        app.router.add_post("/api/text", self._handle_api_text)
        app.router.add_post("/api/pattern", self._handle_api_pattern)
        # Serve any static files from gui/
        app.router.add_static("/gui/", config.GUI_DIR, show_index=True)
        return app

    async def _handle_gui_index(self, request: web.Request) -> web.Response:
        index_path = config.GUI_DIR / "index.html"
        if index_path.exists():
            return web.FileResponse(index_path)
        return web.Response(text="GUI not found", status=404)

    async def _handle_gui_ws(self, request: web.Request) -> web.WebSocketResponse:
        ws = web.WebSocketResponse()
        await ws.prepare(request)

        self._gui_clients.add(ws)
        logger.info(f"🖥️  GUI client connected. Total: {len(self._gui_clients)}")

        # Send full state on connect
        await self._send_full_state_to_gui(ws)

        try:
            async for msg in ws:
                if msg.type == web.WSMsgType.TEXT:
                    try:
                        data = json.loads(msg.data)
                        await self._handle_gui_message(data)
                    except json.JSONDecodeError:
                        pass
                elif msg.type == web.WSMsgType.ERROR:
                    logger.error(f"GUI WS error: {ws.exception()}")
        finally:
            self._gui_clients.discard(ws)
            logger.info(f"🖥️  GUI client disconnected. Total: {len(self._gui_clients)}")

        return ws

    async def _handle_gui_message(self, data: dict):
        """Process messages from GUI WebSocket."""
        msg_type = data.get("type", "")

        if msg_type == "mood_update":
            # Direct config update from sliders
            conf = data.get("config", {})
            await self._send_mood_to_esp32(conf)

        elif msg_type == "set_mood":
            mood_name = data.get("mood", "neutral")
            self.screen.set_mood(mood_name)
            await self.screen.send_mood_update()

        elif msg_type == "set_mode":
            mode = data.get("mode", "IDLE")
            self.screen.set_emotional_state(mode=mode)
            await self.screen.send_mood_update()

        elif msg_type == "particles":
            # Image upload from GUI
            image_b64 = data.get("image", "")
            width = data.get("width", 128)
            height = data.get("height", 128)
            conf = data.get("config", self.screen._build_particle_config())
            if image_b64:
                msg = json.dumps({
                    "type": "particles",
                    "image": image_b64,
                    "width": width,
                    "height": height,
                    "config": conf,
                })
                for ws in self._esp32_clients:
                    try:
                        await ws.send(msg)
                    except Exception:
                        pass

        elif msg_type == "clear":
            await self.screen.send_clear()

        elif msg_type == "pattern":
            pattern_name = data.get("name", "raccoon")
            if pattern_name in PATTERNS:
                img_data = PATTERNS[pattern_name](config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)
                await self.screen.send_particles(img_data, config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)

        elif msg_type == "text_input":
            text = data.get("text", "").strip()
            if text and self.brain:
                asyncio.create_task(self._process_text_input(text))

    async def _send_mood_to_esp32(self, conf: dict):
        """Send a mood config directly to ESP32 (bypasses screen engine state)."""
        msg = json.dumps({"type": "mood", "config": conf})
        now = time.monotonic()
        if now - self._last_mood_send < self._mood_interval:
            return
        self._last_mood_send = now

        dead = set()
        for ws in self._esp32_clients:
            try:
                await ws.send(msg)
            except Exception:
                dead.add(ws)
        for ws in dead:
            self._esp32_clients.discard(ws)

        # Notify GUI
        await self._notify_gui({"type": "mood", "config": conf})

    async def _process_text_input(self, text: str):
        """Process text input from GUI through the brain."""
        if not self.brain:
            return

        self._is_processing = True
        await self.screen.show_thinking(text="thinking...")

        try:
            response_text = ""
            async for chunk in self.brain.think(text):
                response_text += chunk

            # Send response to GUI
            for ws in self._gui_clients:
                try:
                    await ws.send_str(json.dumps({
                        "type": "brain_response",
                        "input": text,
                        "response": response_text,
                    }))
                except Exception:
                    pass

        except Exception as e:
            logger.error(f"Brain processing error: {e}")
        finally:
            self._is_processing = False
            await self.screen.start_ambient()

    # REST API endpoints
    async def _handle_api_state(self, request: web.Request) -> web.Response:
        state = {
            "esp32_connected": len(self._esp32_clients) > 0,
            "esp32_count": len(self._esp32_clients),
            "mode": self.screen.mode,
            "config": self.screen._build_particle_config(),
            "brain_available": self.brain is not None,
            "image_gen_ready": self._image_gen_ready,
        }
        return web.json_response(state)

    async def _handle_api_mood(self, request: web.Request) -> web.Response:
        data = await request.json()
        conf = data.get("config", {})
        await self._send_mood_to_esp32(conf)
        return web.json_response({"ok": True})

    async def _handle_api_particles(self, request: web.Request) -> web.Response:
        data = await request.json()
        image_b64 = data.get("image", "")
        width = data.get("width", 128)
        height = data.get("height", 128)
        if image_b64:
            rgb_data = base64.b64decode(image_b64)
            await self.screen.send_particles(rgb_data, width, height)
        return web.json_response({"ok": True})

    async def _handle_api_clear(self, request: web.Request) -> web.Response:
        await self.screen.send_clear()
        return web.json_response({"ok": True})

    async def _handle_api_mode(self, request: web.Request) -> web.Response:
        data = await request.json()
        mode = data.get("mode", "IDLE")
        self.screen.set_emotional_state(mode=mode)
        await self.screen.send_mood_update()
        return web.json_response({"ok": True})

    async def _handle_api_text(self, request: web.Request) -> web.Response:
        data = await request.json()
        text = data.get("text", "").strip()
        if text and self.brain:
            asyncio.create_task(self._process_text_input(text))
            return web.json_response({"ok": True, "status": "processing"})
        return web.json_response({"ok": False, "error": "No brain or empty text"})

    async def _handle_api_pattern(self, request: web.Request) -> web.Response:
        data = await request.json()
        name = data.get("name", "raccoon")
        if name in PATTERNS:
            img = PATTERNS[name](config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)
            await self.screen.send_particles(img, config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)
            return web.json_response({"ok": True})
        return web.json_response({"ok": False, "error": f"Unknown pattern: {name}"})

    # =========================================================================
    # Server Lifecycle
    # =========================================================================

    async def start(self):
        logger.info("=" * 60)
        logger.info("🦝 Ada Desktop Companion — Starting Up")
        logger.info("=" * 60)

        # Start ESP32 WebSocket server
        esp32_server = await websockets.serve(
            self.handle_esp32,
            config.SCREEN_WS_HOST,
            config.SCREEN_WS_PORT,
            max_size=None,
        )
        logger.info(f"📺 ESP32 WebSocket: ws://0.0.0.0:{config.SCREEN_WS_PORT}")

        # Start HTTP + GUI WebSocket server
        http_app = self._create_http_app()
        runner = web.AppRunner(http_app)
        await runner.setup()
        site = web.TCPSite(runner, config.GUI_HTTP_HOST, config.GUI_HTTP_PORT)
        await site.start()
        logger.info(f"🖥️  Web GUI: http://0.0.0.0:{config.GUI_HTTP_PORT}")
        logger.info(f"🖥️  GUI WebSocket: ws://0.0.0.0:{config.GUI_HTTP_PORT}/ws")

        # Try loading image generation (non-blocking)
        try:
            await image_gen.initialize()
            self._image_gen_ready = await image_gen.is_ready()
        except Exception as e:
            logger.warning(f"🎨 Image gen unavailable: {e}")

        # Background tasks
        tasks = []

        # Idle loop
        tasks.append(asyncio.create_task(self.screen.idle_loop(), name="idle"))

        # Voice pipeline (optional)
        if self.voice:
            tasks.append(asyncio.create_task(self.voice.connect_stt(), name="stt"))
            tasks.append(asyncio.create_task(self.voice.connect_tts(), name="tts"))

        logger.info("")
        logger.info(f"🧠 Brain: {'ready' if self.brain else 'unavailable (no OpenAI key?)'}")
        logger.info(f"🎨 Image Gen: {'ready' if self._image_gen_ready else 'unavailable (no torch)'}")
        logger.info(f"🎤 Voice: {'ready' if self.voice else 'unavailable'}")
        logger.info(f"📺 Default image: raccoon silhouette ({len(self._default_image)} bytes)")
        logger.info("")
        logger.info("🦝 Ada is alive. Pet the belly.")
        logger.info("=" * 60)

        # Wait for shutdown
        try:
            await self._shutdown_event.wait()
        except asyncio.CancelledError:
            pass

        # Cleanup
        logger.info("Shutting down...")
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)

        esp32_server.close()
        await esp32_server.wait_closed()
        await runner.cleanup()

        if self.tools:
            await self.tools.close()
        if self.voice:
            await self.voice.close()
        await image_gen.unload()

        logger.info("🦝 Ada is sleeping. Goodnight.")

    def shutdown(self):
        self._shutdown_event.set()


# =============================================================================
# Entry Point
# =============================================================================

def main():
    server = AdaServer()

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, server.shutdown)

    try:
        loop.run_until_complete(server.start())
    except KeyboardInterrupt:
        logger.info("Interrupted")
    finally:
        loop.close()


if __name__ == "__main__":
    main()
