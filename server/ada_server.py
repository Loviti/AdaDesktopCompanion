"""
Ada Desktop Companion - Main Server

The orchestrator that ties everything together:
- Screen WebSocket (port 8765) — ESP32 belly display
- Audio WebSocket (port 8766) — Microphone/speaker clients
- Voice Pipeline — STT ↔ Brain ↔ TTS
- Screen Engine — Display content generation

This is Ada's nervous system.
"""

import asyncio
import json
import logging
import signal
import sys

import websockets

import config
import image_gen
from ada_brain import AdaBrain
from screen_engine import ScreenEngine
from tool_executor import ToolExecutor
from voice_pipeline import VoicePipeline

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
    """
    Main server orchestrating all of Ada's subsystems.
    """

    def __init__(self):
        # Subsystems
        self.screen = ScreenEngine()
        self.tools = ToolExecutor(screen_engine=self.screen)
        self.brain = AdaBrain(tool_executor=self.tools)
        self.voice = VoicePipeline()

        # State
        self._is_processing: bool = False
        self._shutdown_event = asyncio.Event()

        # Wire up callbacks
        self._setup_callbacks()

        # Image generation (loaded async during start())
        self._image_gen_ready = False

        logger.info("🦝 Ada Desktop Companion server initialized")

    def _setup_callbacks(self):
        """Connect subsystem callbacks."""

        # When mood changes, update screen
        async def on_mood_change(mood: str):
            self.screen.set_mood(mood)
            await self.screen.show_reaction(mood)

        self.brain.on_mood_change = on_mood_change

        # When STT produces a transcript, process it
        async def on_transcript(text: str):
            if self._is_processing:
                logger.debug(f"Ignoring transcript (busy): '{text}'")
                return
            await self._process_voice_input(text)

        self.voice.on_transcript = on_transcript

        # When speech starts, show listening
        async def on_speech_start():
            if not self._is_processing:
                await self.screen.show_listening()

        self.voice.on_speech_start = on_speech_start

        # When speech ends, briefly show thinking
        async def on_speech_end():
            pass  # Transcript callback handles the transition

        self.voice.on_speech_end = on_speech_end

        # When TTS starts playing
        async def on_tts_start():
            await self.screen.show_talking()

        self.voice.on_tts_start = on_tts_start

        # When TTS audio chunk arrives — update visualizer
        async def on_tts_audio(audio_data: bytes):
            # Calculate rough amplitude from PCM data
            try:
                import numpy as np
                samples = np.frombuffer(audio_data, dtype=np.int16)
                amplitude = float(np.abs(samples).mean()) / 32768.0
                await self.screen.update_talk_amplitude(amplitude)
            except Exception:
                pass

        self.voice.on_tts_audio = on_tts_audio

        # When TTS finishes
        async def on_tts_end():
            self._is_processing = False
            await self.screen.start_ambient()

        self.voice.on_tts_end = on_tts_end

    # =========================================================================
    # Voice Processing
    # =========================================================================

    async def _process_voice_input(self, text: str):
        """Full pipeline: transcript → brain → TTS → screen."""
        self._is_processing = True
        logger.info(f"Processing: '{text}'")

        # Show thinking on belly
        await self.screen.show_thinking(text="thinking...")

        try:
            # Collect the brain's response and stream to TTS
            response_text = ""

            async def text_stream():
                nonlocal response_text
                async for chunk in self.brain.think(text):
                    response_text += chunk
                    yield chunk

            await self.voice.synthesize_streaming(text_stream())

            logger.info(f"Response: '{response_text[:100]}...'")

        except Exception as e:
            logger.error(f"Processing error: {e}")
            self._is_processing = False
            await self.screen.show_text("oops, brain error 🦝", style="glitch", mood="chaotic")
            await asyncio.sleep(3)
            await self.screen.start_ambient()

    # =========================================================================
    # WebSocket Handlers
    # =========================================================================

    async def handle_screen_client(self, websocket):
        """Handle an ESP32 screen WebSocket connection."""
        self.screen.subscribe(websocket)

        # Send current state on connect
        await self.screen.start_ambient()

        try:
            async for message in websocket:
                try:
                    data = json.loads(message)
                    msg_type = data.get("type", "")

                    if msg_type == "touch":
                        action = await self.screen.handle_touch(data)
                        if action == "toggle_listening":
                            if self.voice.is_listening:
                                await self.voice.stop_listening()
                                await self.screen.show_text("mic off", style="terminal")
                            else:
                                await self.voice.start_listening()
                                await self.screen.show_listening()

                        elif action == "show_weather":
                            await self.screen.show_thinking("checking weather...")
                            result = await self.tools.execute("get_weather", {})
                            # Weather display handled by tool executor

                    elif msg_type == "ping":
                        await websocket.send(json.dumps({"type": "pong"}))

                except json.JSONDecodeError:
                    logger.warning("Screen client: invalid JSON")
                except Exception as e:
                    logger.error(f"Screen handler error: {e}")

        except websockets.ConnectionClosed:
            pass
        finally:
            self.screen.unsubscribe(websocket)

    async def handle_audio_client(self, websocket):
        """Handle an audio WebSocket connection (mic/speaker client)."""
        self.voice.add_audio_client(websocket)

        # Auto-start listening when audio client connects
        await self.voice.start_listening()
        await self.screen.show_listening()

        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    # Raw audio from microphone — forward to STT
                    await self.voice.send_audio_to_stt(message)

                elif isinstance(message, str):
                    try:
                        data = json.loads(message)
                        msg_type = data.get("type", "")

                        if msg_type == "text":
                            # Text input (alternative to voice)
                            text = data.get("text", "").strip()
                            if text:
                                await self._process_voice_input(text)

                        elif msg_type == "config":
                            # Audio configuration
                            logger.info(f"Audio client config: {data}")

                    except json.JSONDecodeError:
                        pass

        except websockets.ConnectionClosed:
            pass
        finally:
            self.voice.remove_audio_client(websocket)
            if not self.voice._audio_subscribers:
                await self.voice.stop_listening()
                await self.screen.start_ambient()

    # =========================================================================
    # Server Lifecycle
    # =========================================================================

    async def start(self):
        """Start all servers and background tasks."""
        logger.info("=" * 60)
        logger.info("🦝 Ada Desktop Companion — Starting Up")
        logger.info("=" * 60)

        # Start WebSocket servers
        screen_server = await websockets.serve(
            self.handle_screen_client,
            config.SCREEN_WS_HOST,
            config.SCREEN_WS_PORT,
            max_size=None,
        )
        logger.info(f"📺 Screen WebSocket: ws://{config.SCREEN_WS_HOST}:{config.SCREEN_WS_PORT}")

        audio_server = await websockets.serve(
            self.handle_audio_client,
            config.AUDIO_WS_HOST,
            config.AUDIO_WS_PORT,
            max_size=None,
        )
        logger.info(f"🎤 Audio WebSocket: ws://{config.AUDIO_WS_HOST}:{config.AUDIO_WS_PORT}")

        # Initialize image generation (load model into VRAM)
        logger.info("🎨 Loading image generation model into VRAM...")
        try:
            await image_gen.initialize()
            self._image_gen_ready = True
            logger.info("🎨 Image generation ready!")
        except Exception as e:
            logger.warning(f"🎨 Image generation failed to load: {e}")
            logger.warning("   Ada will run without image generation.")

        # Start background tasks
        tasks = [
            asyncio.create_task(self.voice.connect_stt(), name="stt"),
            asyncio.create_task(self.voice.connect_tts(), name="tts"),
            asyncio.create_task(self.screen.idle_loop(), name="idle"),
        ]

        logger.info(f"🔊 STT: {config.STT_WS_URL}")
        logger.info(f"🗣️  TTS: {config.TTS_WS_URL}")
        logger.info(f"🧠 LLM: {config.OPENAI_MODEL} (OpenAI API)")
        logger.info(f"🎨 Image Gen: {config.IMAGE_GEN_MODEL} ({'ready' if self._image_gen_ready else 'unavailable'})")
        logger.info("")
        logger.info("🦝 Ada is alive. Pet the belly.")
        logger.info("=" * 60)

        # Show startup animation on any connected screens
        await self.screen.show_startup()

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

        screen_server.close()
        audio_server.close()
        await screen_server.wait_closed()
        await audio_server.wait_closed()

        await self.voice.close()
        await self.tools.close()
        await image_gen.unload()

        logger.info("🦝 Ada is sleeping. Goodnight.")

    def shutdown(self):
        """Signal the server to shut down."""
        self._shutdown_event.set()


# =============================================================================
# Entry Point
# =============================================================================

def main():
    server = AdaServer()

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    # Handle signals
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
