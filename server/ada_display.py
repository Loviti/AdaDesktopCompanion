#!/usr/bin/env python3
"""
Ada Display Control CLI

Send state updates to the Ada particle display.

Usage:
    # Set formation
    ./ada_display.py --formation sun
    
    # Set mood
    ./ada_display.py --valence 0.5 --arousal 0.7
    
    # Combined
    ./ada_display.py --formation heart --valence 0.9 --arousal 0.6
    
    # Set brightness
    ./ada_display.py --brightness 200
"""

import asyncio
import json
import argparse
import sys
import websockets

DEFAULT_SERVER = "ws://localhost:8765"

async def send_state(server: str, **kwargs):
    """Send state update to display server."""
    try:
        async with websockets.connect(server, close_timeout=2) as ws:
            # Build state message
            msg = {"type": "state"}
            
            if "valence" in kwargs or "arousal" in kwargs:
                msg["mood"] = {
                    "valence": kwargs.get("valence", 0.0),
                    "arousal": kwargs.get("arousal", 0.3)
                }
            
            if "formation" in kwargs and kwargs["formation"]:
                msg["formation"] = kwargs["formation"]
                
            if "particle_count" in kwargs and kwargs["particle_count"]:
                msg["particle_count"] = kwargs["particle_count"]
                
            if "transition_ms" in kwargs and kwargs["transition_ms"]:
                msg["transition_ms"] = kwargs["transition_ms"]
            
            await ws.send(json.dumps(msg))
            print(f"Sent: {json.dumps(msg, indent=2)}")
            
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

async def send_config(server: str, brightness: int):
    """Send config update to display server."""
    try:
        async with websockets.connect(server, close_timeout=2) as ws:
            msg = {"type": "config", "brightness": brightness}
            await ws.send(json.dumps(msg))
            print(f"Sent: {json.dumps(msg)}")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Control Ada particle display")
    parser.add_argument("--server", default=DEFAULT_SERVER, help="WebSocket server URL")
    
    # State options
    parser.add_argument("--formation", "-f", 
                       choices=["idle", "cloud", "sun", "rain", "snow", 
                               "heart", "thinking", "wave"],
                       help="Particle formation")
    parser.add_argument("--valence", "-v", type=float,
                       help="Mood valence (-1.0 to 1.0)")
    parser.add_argument("--arousal", "-a", type=float,
                       help="Mood arousal (0.0 to 1.0)")
    parser.add_argument("--particles", "-p", type=int,
                       help="Particle count (50-400)")
    parser.add_argument("--transition", "-t", type=int,
                       help="Transition time in ms")
    
    # Config options
    parser.add_argument("--brightness", "-b", type=int,
                       help="Display brightness (0-255)")
    
    # Presets
    parser.add_argument("--happy", action="store_true", help="Happy preset")
    parser.add_argument("--calm", action="store_true", help="Calm preset")
    parser.add_argument("--thinking", action="store_true", help="Thinking preset")
    parser.add_argument("--love", action="store_true", help="Love preset")
    
    args = parser.parse_args()
    
    # Handle presets
    if args.happy:
        args.formation = "sun" if not args.formation else args.formation
        args.valence = 0.8
        args.arousal = 0.7
    elif args.calm:
        args.formation = "wave" if not args.formation else args.formation
        args.valence = 0.2
        args.arousal = 0.2
    elif args.thinking:
        args.formation = "thinking"
        args.valence = 0.0
        args.arousal = 0.5
    elif args.love:
        args.formation = "heart"
        args.valence = 0.9
        args.arousal = 0.6
        args.transition = args.transition or 1500
    
    # Handle brightness separately
    if args.brightness is not None:
        asyncio.run(send_config(args.server, args.brightness))
        return
    
    # Check if any state options provided
    if not any([args.formation, args.valence is not None, args.arousal is not None,
                args.particles, args.transition]):
        parser.print_help()
        sys.exit(1)
    
    # Send state update
    asyncio.run(send_state(
        args.server,
        formation=args.formation,
        valence=args.valence,
        arousal=args.arousal,
        particle_count=args.particles,
        transition_ms=args.transition
    ))

if __name__ == "__main__":
    main()
