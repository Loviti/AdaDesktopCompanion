#!/usr/bin/env python3
"""
Ada Display State Server

Bridges between Ada (OpenClaw) and the ESP32 particle display.
Accepts state updates and forwards them to connected displays.

Usage:
    python display_server.py [--port 8765]

Protocol:
    - ESP32 connects and sends: {"type":"hello","mode":"native",...}
    - Ada sends state updates: {"type":"state","mood":{...},"formation":"..."}
    - Server forwards state to all connected displays
"""

import asyncio
import json
import logging
import argparse
from datetime import datetime
from typing import Set, Optional
import websockets
from websockets.server import WebSocketServerProtocol

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger(__name__)

# Connected clients
display_clients: Set[WebSocketServerProtocol] = set()
control_clients: Set[WebSocketServerProtocol] = set()

# Current state (for new connections)
current_state = {
    "type": "state",
    "mood": {
        "valence": 0.0,
        "arousal": 0.3
    },
    "formation": "idle",
    "particle_count": 300
}

async def broadcast_to_displays(message: str):
    """Send message to all connected displays."""
    if display_clients:
        await asyncio.gather(
            *[client.send(message) for client in display_clients],
            return_exceptions=True
        )

async def handle_display_message(websocket: WebSocketServerProtocol, data: dict):
    """Handle messages from ESP32 display."""
    msg_type = data.get("type", "")
    
    if msg_type == "hello":
        mode = data.get("mode", "unknown")
        width = data.get("width", 0)
        height = data.get("height", 0)
        logger.info(f"Display connected: {mode} mode, {width}x{height}")
        
        # Send current state to newly connected display
        await websocket.send(json.dumps(current_state))
        
    elif msg_type == "ping":
        await websocket.send(json.dumps({"type": "pong"}))
        
    elif msg_type == "touch":
        # Forward touch events to control clients (Ada)
        x = data.get("x", 0)
        y = data.get("y", 0)
        logger.info(f"Touch event: ({x}, {y})")
        # Could forward to Ada here
        
    else:
        logger.debug(f"Unknown display message: {msg_type}")

async def handle_control_message(websocket: WebSocketServerProtocol, data: dict):
    """Handle messages from control clients (Ada)."""
    global current_state
    
    msg_type = data.get("type", "")
    
    if msg_type == "state":
        # Update and broadcast state
        if "mood" in data:
            current_state["mood"] = data["mood"]
        if "formation" in data:
            current_state["formation"] = data["formation"]
        if "particle_count" in data:
            current_state["particle_count"] = data["particle_count"]
        if "transition_ms" in data:
            current_state["transition_ms"] = data["transition_ms"]
            
        logger.info(f"State update: formation={current_state['formation']}, "
                   f"valence={current_state['mood']['valence']:.2f}, "
                   f"arousal={current_state['mood']['arousal']:.2f}")
        
        await broadcast_to_displays(json.dumps(current_state))
        
    elif msg_type == "config":
        # Forward config directly to displays
        await broadcast_to_displays(json.dumps(data))
        logger.info(f"Config update: {data}")
        
    elif msg_type == "ping":
        await websocket.send(json.dumps({"type": "pong"}))
        
    else:
        logger.debug(f"Unknown control message: {msg_type}")

async def handler(websocket: WebSocketServerProtocol, path: str):
    """Main WebSocket handler."""
    client_type = None
    
    try:
        # Wait for first message to determine client type
        async for message in websocket:
            try:
                data = json.loads(message)
            except json.JSONDecodeError:
                logger.warning(f"Invalid JSON: {message[:100]}")
                continue
            
            # Determine client type from first message
            if client_type is None:
                if data.get("type") == "hello" and data.get("mode") in ("native", "stream"):
                    client_type = "display"
                    display_clients.add(websocket)
                    logger.info(f"Display client connected ({len(display_clients)} total)")
                else:
                    client_type = "control"
                    control_clients.add(websocket)
                    logger.info(f"Control client connected ({len(control_clients)} total)")
            
            # Route message to appropriate handler
            if client_type == "display":
                await handle_display_message(websocket, data)
            else:
                await handle_control_message(websocket, data)
                
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        # Clean up on disconnect
        if client_type == "display":
            display_clients.discard(websocket)
            logger.info(f"Display disconnected ({len(display_clients)} remaining)")
        elif client_type == "control":
            control_clients.discard(websocket)
            logger.info(f"Control client disconnected ({len(control_clients)} remaining)")

async def status_reporter():
    """Periodically log server status."""
    while True:
        await asyncio.sleep(60)
        logger.info(f"Status: {len(display_clients)} displays, "
                   f"{len(control_clients)} controllers connected")

async def main(host: str, port: int):
    """Start the server."""
    logger.info(f"Starting Ada Display Server on {host}:{port}")
    logger.info("Waiting for connections...")
    
    # Start status reporter
    asyncio.create_task(status_reporter())
    
    # Start WebSocket server
    async with websockets.serve(handler, host, port):
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Ada Display State Server")
    parser.add_argument("--host", default="0.0.0.0", help="Host to bind to")
    parser.add_argument("--port", type=int, default=8765, help="Port to listen on")
    args = parser.parse_args()
    
    try:
        asyncio.run(main(args.host, args.port))
    except KeyboardInterrupt:
        logger.info("Server stopped")
