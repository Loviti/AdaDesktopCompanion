#!/usr/bin/env bash
# Ada Desktop Companion - Server Launcher
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"

# Create venv if needed
if [ ! -d "$VENV_DIR" ]; then
    echo "🦝 Creating virtual environment..."
    python3 -m venv "$VENV_DIR"
    source "$VENV_DIR/bin/activate"
    pip install --upgrade pip
    pip install -r "$SCRIPT_DIR/requirements.txt"
else
    source "$VENV_DIR/bin/activate"
fi

# Load .env if present
if [ -f "$SCRIPT_DIR/.env" ]; then
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
fi

echo "🦝 Starting Ada Desktop Companion server..."
echo "   ESP32 WebSocket: ws://0.0.0.0:8765"
echo "   Web GUI:         http://0.0.0.0:8766"
echo ""

cd "$SCRIPT_DIR"
exec python3 ada_server.py "$@"
