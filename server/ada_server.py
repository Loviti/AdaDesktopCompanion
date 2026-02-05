"""
Ada Desktop Companion - Unified Streaming Server

Architecture:
  - Port 8765: WebSocket for ESP32 (binary JPEG frames + JSON commands)
  - Port 8766: HTTP for web GUI + WebSocket at /ws for GUI live updates

The server renders particles and streams JPEG frames to the ESP32.
The ESP32 is just a display — all computation happens here.
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
from screen_engine import ScreenEngine
from particle_renderer import ParticleRenderer
from test_patterns import get_default_image, PATTERNS

# Optional imports
try:
    from ada_brain import AdaBrain
    from tool_executor import ToolExecutor
    _brain_available = True
except Exception as e:
    _brain_available = False
    logging.getLogger("ada.server").warning(f"Brain unavailable: {e}")

try:
    from voice_pipeline import VoicePipeline
    _voice_available = True
except Exception:
    _voice_available = False

try:
    import image_gen
    _image_gen_available = True
except Exception:
    _image_gen_available = False

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
        self.renderer = ParticleRenderer(466, 466)

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

        self._image_gen_ready = False

        # ESP32 connections
        self._esp32_clients: set = set()
        self._esp32_stream_mode: dict = {}  # ws -> mode ("stream" or "legacy")

        # GUI WebSocket connections
        self._gui_clients: set = set()

        # Streaming state
        self._streaming = False
        self._target_fps = 15
        self._frame_count = 0
        self._last_fps_report = time.monotonic()

        # Wire screen engine callbacks
        self._setup_screen_callbacks()

        logger.info("🦝 Ada server initialized")

    def _setup_screen_callbacks(self):
        """When screen engine updates mood, push config to renderer."""
        original_send_mood = self.screen.send_mood_update

        async def on_mood_update():
            # Update renderer config from screen engine's particle config
            config_dict = self.screen._build_particle_config()
            self.renderer.update_config(config_dict)
            # Also notify GUI clients
            await self._broadcast_gui({
                "type": "state",
                "config": config_dict,
                "mode": self.screen.mode,
            })

        self.screen.send_mood_update = on_mood_update

        # Override send_particles to feed image to renderer
        original_send_particles = self.screen.send_particles

        async def on_send_particles(image_data, width, height):
            self.renderer.create_from_image(image_data, width, height)
            config_dict = self.screen._build_particle_config()
            self.renderer.update_config(config_dict)

        self.screen.send_particles = on_send_particles

    # =========================================================================
    # ESP32 WebSocket Handler (Port 8765)
    # =========================================================================

    async def handle_esp32(self, websocket, path=None):
        """Handle ESP32 WebSocket connection."""
        self._esp32_clients.add(websocket)
        self._esp32_stream_mode[websocket] = "stream"  # Default to stream mode
        remote = websocket.remote_address
        logger.info(f"[ESP32] Connected from {remote}. Total: {len(self._esp32_clients)}")

        # Initialize renderer with default particles if none exist
        if not self.renderer.particles:
            default_img = get_default_image(config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)
            self.renderer.create_from_image(
                default_img, config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT
            )
            initial_config = self.screen._build_particle_config()
            self.renderer.update_config(initial_config)

        # Start streaming if not already
        if not self._streaming:
            self._streaming = True
            asyncio.create_task(self._stream_loop())

        try:
            async for message in websocket:
                try:
                    if isinstance(message, str):
                        data = json.loads(message)
                        await self._handle_esp32_message(websocket, data)
                except json.JSONDecodeError:
                    pass
                except Exception as e:
                    logger.error(f"[ESP32] Message error: {e}")
        except websockets.ConnectionClosed:
            pass
        finally:
            self._esp32_clients.discard(websocket)
            self._esp32_stream_mode.pop(websocket, None)
            logger.info(f"[ESP32] Disconnected. Total: {len(self._esp32_clients)}")

            # Stop streaming if no clients
            if not self._esp32_clients:
                self._streaming = False

    async def _handle_esp32_message(self, ws, data: dict):
        """Handle messages from ESP32."""
        msg_type = data.get("type", "")

        if msg_type == "hello":
            mode = data.get("mode", "stream")
            self._esp32_stream_mode[ws] = mode
            logger.info(f"[ESP32] Hello: mode={mode}, "
                       f"width={data.get('width')}, height={data.get('height')}")

        elif msg_type == "ping":
            try:
                await ws.send(json.dumps({"type": "pong"}))
            except Exception:
                pass

        elif msg_type == "touch":
            logger.info(f"[ESP32] Touch: ({data.get('x')}, {data.get('y')})")
            # Forward to GUI
            await self._broadcast_gui({"type": "touch", "x": data.get("x"), "y": data.get("y")})

    # =========================================================================
    # Frame Streaming Loop
    # =========================================================================

    async def _stream_loop(self):
        """Main render loop — updates physics and streams frames to ESP32."""
        logger.info(f"Streaming started at {self._target_fps} FPS target")
        last_time = time.monotonic()
        frame_interval = 1.0 / self._target_fps

        while self._streaming and self._esp32_clients:
            now = time.monotonic()
            dt = now - last_time
            last_time = now

            # Update particle physics
            self.renderer.update(min(dt, 0.1))

            # Render frame to JPEG
            try:
                jpeg_data = self.renderer.render_frame()
            except Exception as e:
                logger.error(f"Render error: {e}")
                await asyncio.sleep(frame_interval)
                continue

            # Send to all ESP32 clients as binary
            dead = set()
            for ws in self._esp32_clients:
                try:
                    await ws.send(jpeg_data)
                except Exception:
                    dead.add(ws)
            for ws in dead:
                self._esp32_clients.discard(ws)
                self._esp32_stream_mode.pop(ws, None)

            self._frame_count += 1

            # FPS reporting
            if now - self._last_fps_report >= 10.0:
                fps = self._frame_count / (now - self._last_fps_report)
                size_kb = len(jpeg_data) / 1024 if jpeg_data else 0
                logger.info(f"Stream: {fps:.1f} FPS, {size_kb:.0f} KB/frame, "
                           f"{self.renderer.get_particle_count()} particles, "
                           f"{len(self._esp32_clients)} clients")
                self._frame_count = 0
                self._last_fps_report = now

            # Sleep to maintain target FPS
            elapsed = time.monotonic() - now
            sleep_time = frame_interval - elapsed
            if sleep_time > 0:
                await asyncio.sleep(sleep_time)
            else:
                await asyncio.sleep(0)  # Yield to event loop

        logger.info("Streaming stopped")
        self._streaming = False

    # =========================================================================
    # GUI WebSocket Handler (Port 8766 /ws)
    # =========================================================================

    async def handle_gui_ws(self, request):
        """Handle GUI WebSocket upgrade."""
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        self._gui_clients.add(ws)
        logger.info(f"[GUI] Connected. Total: {len(self._gui_clients)}")

        # Send initial state
        try:
            await ws.send_json({
                "type": "full_state",
                "config": self.renderer.config.to_dict(),
                "target_config": self.renderer.target_config.to_dict(),
                "mode": self.screen.mode,
                "esp32_connected": len(self._esp32_clients) > 0,
                "esp32_count": len(self._esp32_clients),
                "streaming": self._streaming,
                "target_fps": self._target_fps,
                "patterns": list(PATTERNS.keys()),
                "brain_available": self.brain is not None,
            })
        except Exception:
            pass

        try:
            async for msg in ws:
                if msg.type == web.WSMsgType.TEXT:
                    try:
                        data = json.loads(msg.data)
                        await self._handle_gui_message(data)
                    except json.JSONDecodeError:
                        pass
                    except Exception as e:
                        logger.error(f"[GUI] Error: {e}")
        finally:
            self._gui_clients.discard(ws)
            logger.info(f"[GUI] Disconnected. Total: {len(self._gui_clients)}")

        return ws

    async def _handle_gui_message(self, data: dict):
        """Handle commands from the web GUI."""
        cmd = data.get("cmd") or data.get("type")

        if cmd == "set_mode":
            mode = data.get("mode", "IDLE")
            self.screen.set_emotional_state(mode=mode)
            await self.screen.send_mood_update()

        elif cmd == "set_emotion":
            self.screen.set_emotion(
                data.get("valence", 0),
                data.get("arousal", 0.3),
                data.get("certainty", 0.8),
            )
            await self.screen.send_mood_update()

        elif cmd == "set_mood":
            self.screen.set_mood(data.get("mood", "neutral"))
            await self.screen.send_mood_update()

        elif cmd == "set_config":
            # Direct config override from GUI sliders
            config_update = data.get("config", {})
            self.renderer.update_config(config_update)
            self.screen._build_particle_config()  # Keep screen engine in sync
            await self._broadcast_gui({"type": "state", "config": self.renderer.target_config.to_dict()})

        elif cmd == "set_gaze":
            pass  # Not used in streaming mode

        elif cmd == "blink":
            pass  # Not used in streaming mode

        elif cmd == "load_pattern":
            pattern_name = data.get("pattern", "raccoon")
            if pattern_name in PATTERNS:
                img_data = PATTERNS[pattern_name](config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)
                self.renderer.create_from_image(img_data, config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)
                logger.info(f"Loaded pattern: {pattern_name}")

        elif cmd == "upload_image":
            # Base64 RGB image from GUI
            image_b64 = data.get("image", "")
            width = data.get("width", 64)
            height = data.get("height", 64)
            if image_b64:
                try:
                    rgb_data = base64.b64decode(image_b64)
                    self.renderer.create_from_image(rgb_data, width, height)
                    logger.info(f"Uploaded image: {width}x{height}")
                except Exception as e:
                    logger.error(f"Image upload error: {e}")

        elif cmd == "clear":
            self.renderer.clear()

        elif cmd == "set_fps":
            self._target_fps = max(1, min(30, data.get("fps", 15)))
            logger.info(f"Target FPS: {self._target_fps}")

        elif cmd == "set_quality":
            self.renderer.jpeg_quality = max(30, min(95, data.get("quality", 80)))
            logger.info(f"JPEG quality: {self.renderer.jpeg_quality}")

        elif cmd == "chat":
            text = data.get("text", "").strip()
            if text and self.brain:
                asyncio.create_task(self._process_chat(text))

    async def _process_chat(self, text: str):
        """Process chat from GUI through brain."""
        if not self.brain:
            return

        self.screen.set_emotional_state(mode="THINKING")
        await self.screen.send_mood_update()

        try:
            response = ""
            async for chunk in self.brain.think(text):
                response += chunk

            self.screen.set_emotional_state(mode="TALKING")
            await self.screen.send_mood_update()

            await self._broadcast_gui({
                "type": "chat_response",
                "text": response,
            })

            await asyncio.sleep(2)
            self.screen.set_emotional_state(mode="IDLE")
            await self.screen.send_mood_update()

        except Exception as e:
            logger.error(f"Chat error: {e}")
            await self._broadcast_gui({"type": "chat_response", "text": f"Error: {e}"})

    async def _broadcast_gui(self, message: dict):
        """Send message to all GUI WebSocket clients."""
        dead = set()
        for ws in self._gui_clients:
            try:
                await ws.send_json(message)
            except Exception:
                dead.add(ws)
        self._gui_clients -= dead

    # =========================================================================
    # HTTP Routes
    # =========================================================================

    def setup_http(self) -> web.Application:
        app = web.Application()
        gui_dir = Path(__file__).parent / "gui"

        async def index(request):
            return web.FileResponse(gui_dir / "index.html")

        async def api_state(request):
            return web.json_response({
                "esp32_connected": len(self._esp32_clients) > 0,
                "esp32_count": len(self._esp32_clients),
                "mode": self.screen.mode,
                "config": self.renderer.config.to_dict(),
                "target_config": self.renderer.target_config.to_dict(),
                "streaming": self._streaming,
                "target_fps": self._target_fps,
                "jpeg_quality": self.renderer.jpeg_quality,
                "particle_count": self.renderer.get_particle_count(),
                "brain_available": self.brain is not None,
                "patterns": list(PATTERNS.keys()),
            })

        async def api_command(request):
            data = await request.json()
            await self._handle_gui_message(data)
            return web.json_response({"ok": True})

        async def api_frame(request):
            """Get current frame as JPEG (for debugging / preview)."""
            jpeg = self.renderer.render_frame()
            return web.Response(body=jpeg, content_type="image/jpeg")

        async def static_file(request):
            filename = request.match_info["filename"]
            filepath = gui_dir / filename
            if filepath.exists() and filepath.is_relative_to(gui_dir):
                return web.FileResponse(filepath)
            return web.Response(status=404)

        app.router.add_get("/", index)
        app.router.add_get("/ws", self.handle_gui_ws)
        app.router.add_get("/api/state", api_state)
        app.router.add_post("/api/command", api_command)
        app.router.add_get("/api/frame", api_frame)
        app.router.add_get("/{filename}", static_file)

        return app

    # =========================================================================
    # Server Lifecycle
    # =========================================================================

    async def start(self):
        self._shutdown_event = asyncio.Event()

        logger.info("=" * 60)
        logger.info("🦝 Ada Desktop Companion — Starting Up")
        logger.info("=" * 60)

        # Start ESP32 WebSocket server
        ws_server = await websockets.serve(
            self.handle_esp32,
            config.SCREEN_WS_HOST,
            config.SCREEN_WS_PORT,
            max_size=None,
            ping_interval=20,
            ping_timeout=60,
        )
        logger.info(f"📺 ESP32 WebSocket: ws://0.0.0.0:{config.SCREEN_WS_PORT}")

        # Start HTTP + GUI WebSocket server
        app = self.setup_http()
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, "0.0.0.0", config.GUI_HTTP_PORT)
        await site.start()
        logger.info(f"🖥️  Web GUI: http://0.0.0.0:{config.GUI_HTTP_PORT}")
        logger.info(f"🖥️  GUI WebSocket: ws://0.0.0.0:{config.GUI_HTTP_PORT}/ws")

        # Init image gen (optional)
        if _image_gen_available:
            try:
                await image_gen.initialize()
                self._image_gen_ready = True
                logger.info("🎨 Image generation ready")
            except Exception as e:
                logger.info(f"🎨 Image gen unavailable: {e}")

        # Create initial particles
        default_img = get_default_image(config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)
        self.renderer.create_from_image(default_img, config.IMAGE_GEN_WIDTH, config.IMAGE_GEN_HEIGHT)
        initial_config = self.screen._build_particle_config()
        self.renderer.update_config(initial_config)

        logger.info(f"📺 Default image: {config.IMAGE_GEN_WIDTH}x{config.IMAGE_GEN_HEIGHT}")
        logger.info(f"🎬 Stream target: {self._target_fps} FPS, JPEG Q{self.renderer.jpeg_quality}")
        logger.info(f"🧠 Brain: {'ready' if self.brain else 'unavailable'}")
        logger.info("")
        logger.info("🦝 Ada is alive. Pet the belly.")
        logger.info("=" * 60)

        # Background tasks
        tasks = []
        if self.voice:
            tasks.append(asyncio.create_task(self.voice.connect_stt(), name="stt"))
            tasks.append(asyncio.create_task(self.voice.connect_tts(), name="tts"))

        try:
            await self._shutdown_event.wait()
        except asyncio.CancelledError:
            pass

        logger.info("Shutting down...")
        self._streaming = False
        for task in tasks:
            task.cancel()
        ws_server.close()
        await ws_server.wait_closed()
        await runner.cleanup()

    def shutdown(self):
        self._shutdown_event.set()


def main():
    server = AdaServer()
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, server.shutdown)

    try:
        loop.run_until_complete(server.start())
    except KeyboardInterrupt:
        pass
    finally:
        loop.close()


if __name__ == "__main__":
    main()
