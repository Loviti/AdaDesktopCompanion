"""
Ada Desktop Companion - Voice Pipeline

STT (ws:8090) → LLM (OpenAI streaming) → TTS (ws:8089)

Handles the full voice conversation loop:
1. Receive raw audio from client via WebSocket (port 8766)
2. Forward to Kyutai DSM STT server (ws:8090)
3. When transcription arrives, feed to Ada's brain
4. Stream brain output to TTS server (ws:8089)
5. Forward TTS audio back to client

Uses asyncio for concurrent STT/TTS streaming.
"""

import asyncio
import json
import logging
import struct
import time
from typing import Optional, Callable, Awaitable

import numpy as np
import websockets

import config

logger = logging.getLogger("ada.voice")


class VoicePipeline:
    """
    Full-duplex voice pipeline: Audio In → STT → Brain → TTS → Audio Out.
    """

    def __init__(self):
        self._stt_ws: Optional[websockets.WebSocketClientProtocol] = None
        self._tts_ws: Optional[websockets.WebSocketClientProtocol] = None
        self._audio_subscribers: set = set()  # Client audio WebSockets

        # Callbacks (set by ada_server)
        self.on_transcript: Optional[Callable[[str], Awaitable[None]]] = None
        self.on_speech_start: Optional[Callable[[], Awaitable[None]]] = None
        self.on_speech_end: Optional[Callable[[], Awaitable[None]]] = None
        self.on_tts_audio: Optional[Callable[[bytes], Awaitable[None]]] = None
        self.on_tts_start: Optional[Callable[[], Awaitable[None]]] = None
        self.on_tts_end: Optional[Callable[[], Awaitable[None]]] = None

        self._is_listening: bool = False
        self._stt_connected: bool = False
        self._tts_connected: bool = False
        self._reconnect_delay: float = 2.0

    # =========================================================================
    # Audio Client Management
    # =========================================================================

    def add_audio_client(self, ws):
        """Register an audio WebSocket client."""
        self._audio_subscribers.add(ws)
        logger.info(f"Audio client connected. Total: {len(self._audio_subscribers)}")

    def remove_audio_client(self, ws):
        """Remove an audio WebSocket client."""
        self._audio_subscribers.discard(ws)
        logger.info(f"Audio client disconnected. Total: {len(self._audio_subscribers)}")

    async def broadcast_audio(self, audio_data: bytes):
        """Send audio to all connected audio clients."""
        if not self._audio_subscribers:
            return
        dead = set()
        for ws in self._audio_subscribers:
            try:
                await ws.send(audio_data)
            except Exception:
                dead.add(ws)
        for ws in dead:
            self._audio_subscribers.discard(ws)

    # =========================================================================
    # STT Connection
    # =========================================================================

    async def connect_stt(self):
        """Connect to the Kyutai DSM STT WebSocket server."""
        while True:
            try:
                logger.info(f"Connecting to STT at {config.STT_WS_URL}...")
                self._stt_ws = await websockets.connect(
                    config.STT_WS_URL,
                    max_size=None,
                    ping_interval=20,
                    ping_timeout=10,
                )
                self._stt_connected = True
                logger.info("STT connected ✓")

                # Listen for transcription events
                await self._stt_receive_loop()

            except websockets.ConnectionClosed as e:
                logger.warning(f"STT connection closed: {e}")
            except ConnectionRefusedError:
                logger.warning(f"STT server not available at {config.STT_WS_URL}")
            except Exception as e:
                logger.error(f"STT connection error: {e}")

            self._stt_connected = False
            logger.info(f"STT reconnecting in {self._reconnect_delay}s...")
            await asyncio.sleep(self._reconnect_delay)

    async def _stt_receive_loop(self):
        """Listen for transcription events from STT server."""
        async for message in self._stt_ws:
            try:
                if isinstance(message, str):
                    data = json.loads(message)
                    await self._handle_stt_event(data)
                # Binary messages are echoed audio — ignore
            except json.JSONDecodeError:
                logger.warning(f"STT: non-JSON text message")
            except Exception as e:
                logger.error(f"STT event error: {e}")

    async def _handle_stt_event(self, event: dict):
        """Process STT transcription events."""
        event_type = event.get("type", "")

        if event_type == "transcript" or "text" in event:
            text = event.get("text", event.get("transcript", "")).strip()
            if text and self.on_transcript:
                logger.info(f"STT transcript: '{text}'")
                await self.on_transcript(text)

        elif event_type == "speech_start":
            logger.debug("STT: speech started")
            if self.on_speech_start:
                await self.on_speech_start()

        elif event_type == "speech_end":
            logger.debug("STT: speech ended")
            if self.on_speech_end:
                await self.on_speech_end()

        elif event_type == "partial":
            # Partial transcription — could update screen
            partial = event.get("text", "").strip()
            if partial:
                logger.debug(f"STT partial: '{partial}'")

    async def send_audio_to_stt(self, audio_data: bytes):
        """Send raw audio to the STT server."""
        if self._stt_ws and self._stt_connected:
            try:
                await self._stt_ws.send(audio_data)
            except Exception as e:
                logger.error(f"STT send error: {e}")
                self._stt_connected = False

    # =========================================================================
    # TTS Connection
    # =========================================================================

    async def connect_tts(self):
        """Connect to the Kyutai DSM TTS WebSocket server."""
        while True:
            try:
                logger.info(f"Connecting to TTS at {config.TTS_WS_URL}...")
                self._tts_ws = await websockets.connect(
                    config.TTS_WS_URL,
                    max_size=None,
                    ping_interval=20,
                    ping_timeout=10,
                )
                self._tts_connected = True
                logger.info("TTS connected ✓")

                # Listen for audio output
                await self._tts_receive_loop()

            except websockets.ConnectionClosed as e:
                logger.warning(f"TTS connection closed: {e}")
            except ConnectionRefusedError:
                logger.warning(f"TTS server not available at {config.TTS_WS_URL}")
            except Exception as e:
                logger.error(f"TTS connection error: {e}")

            self._tts_connected = False
            logger.info(f"TTS reconnecting in {self._reconnect_delay}s...")
            await asyncio.sleep(self._reconnect_delay)

    async def _tts_receive_loop(self):
        """Listen for audio chunks from TTS server."""
        is_speaking = False

        async for message in self._tts_ws:
            try:
                if isinstance(message, bytes):
                    # Audio chunk — forward to clients
                    if not is_speaking:
                        is_speaking = True
                        if self.on_tts_start:
                            await self.on_tts_start()

                    await self.broadcast_audio(message)

                    if self.on_tts_audio:
                        await self.on_tts_audio(message)

                elif isinstance(message, str):
                    data = json.loads(message)
                    if data.get("type") == "done" or data.get("done"):
                        if is_speaking:
                            is_speaking = False
                            if self.on_tts_end:
                                await self.on_tts_end()
            except Exception as e:
                logger.error(f"TTS receive error: {e}")

    async def synthesize(self, text: str):
        """Send text to TTS for synthesis."""
        if not self._tts_ws or not self._tts_connected:
            logger.warning("TTS not connected, cannot synthesize")
            return

        try:
            # Send text as JSON command
            await self._tts_ws.send(json.dumps({
                "type": "synthesize",
                "text": text,
            }))
            logger.info(f"TTS synthesizing: '{text[:50]}...'")
        except Exception as e:
            logger.error(f"TTS send error: {e}")
            self._tts_connected = False

    async def synthesize_streaming(self, text_generator):
        """
        Stream text chunks to TTS as they arrive from the LLM.
        Accumulates text into sentence-sized chunks for natural speech.
        """
        if not self._tts_ws or not self._tts_connected:
            logger.warning("TTS not connected, cannot synthesize")
            return

        buffer = ""
        sentence_delimiters = {'.', '!', '?', ':', ';', '\n'}
        min_chunk_len = 20  # Don't send tiny fragments

        async for chunk in text_generator:
            buffer += chunk

            # Check if we have a complete sentence/phrase
            should_flush = False
            for delim in sentence_delimiters:
                if delim in buffer and len(buffer) >= min_chunk_len:
                    should_flush = True
                    break

            if should_flush:
                # Find the last sentence boundary
                last_boundary = -1
                for delim in sentence_delimiters:
                    idx = buffer.rfind(delim)
                    if idx > last_boundary:
                        last_boundary = idx

                if last_boundary > 0:
                    to_send = buffer[:last_boundary + 1].strip()
                    buffer = buffer[last_boundary + 1:]

                    if to_send:
                        await self.synthesize(to_send)

        # Flush remaining buffer
        if buffer.strip():
            await self.synthesize(buffer.strip())

    # =========================================================================
    # Pipeline Control
    # =========================================================================

    async def start_listening(self):
        """Enable STT processing."""
        self._is_listening = True
        logger.info("Listening started")

    async def stop_listening(self):
        """Disable STT processing."""
        self._is_listening = False
        logger.info("Listening stopped")

    @property
    def is_listening(self) -> bool:
        return self._is_listening

    @property
    def stt_ready(self) -> bool:
        return self._stt_connected

    @property
    def tts_ready(self) -> bool:
        return self._tts_connected

    async def close(self):
        """Close all connections."""
        if self._stt_ws:
            await self._stt_ws.close()
        if self._tts_ws:
            await self._tts_ws.close()
        logger.info("Voice pipeline closed")
