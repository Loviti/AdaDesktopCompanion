#!/usr/bin/env bash
# =============================================================================
# Ada Desktop Companion - Launch Script
# =============================================================================
# Starts Ada's brain server. Expects STT/TTS (moshi-server) to be running.
#
# Usage:
#   ./start.sh          # Normal start
#   ./start.sh --dev    # Debug logging
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${CYAN}"
echo "  🦝 Ada Desktop Companion"
echo "  ========================"
echo -e "${NC}"

# =============================================================================
# Environment
# =============================================================================

# Load .env if it exists
if [ -f "$SCRIPT_DIR/.env" ]; then
    echo -e "${GREEN}Loading .env${NC}"
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
fi

# Dev mode
if [ "${1:-}" = "--dev" ]; then
    export LOG_LEVEL="DEBUG"
    echo -e "${YELLOW}Debug mode enabled${NC}"
fi

# =============================================================================
# Checks
# =============================================================================

# Check Python
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Python 3 not found!${NC}"
    exit 1
fi

# Check OpenAI key
if [ -z "${OPENAI_API_KEY:-}" ]; then
    echo -e "${YELLOW}Warning: OPENAI_API_KEY not set. LLM won't work.${NC}"
    echo "  Set it: export OPENAI_API_KEY=sk-..."
    echo "  Or add to server/.env"
fi

# Check if STT is running
if curl -s --max-time 1 http://localhost:8090 > /dev/null 2>&1 || \
   python3 -c "import socket; s=socket.socket(); s.settimeout(1); s.connect(('localhost',8090)); s.close()" 2>/dev/null; then
    echo -e "${GREEN}✓ STT server (port 8090)${NC}"
else
    echo -e "${YELLOW}⚠ STT server not detected on port 8090${NC}"
    echo "  Ada will auto-reconnect when it's available."
fi

# Check if TTS is running
if curl -s --max-time 1 http://localhost:8089 > /dev/null 2>&1 || \
   python3 -c "import socket; s=socket.socket(); s.settimeout(1); s.connect(('localhost',8089)); s.close()" 2>/dev/null; then
    echo -e "${GREEN}✓ TTS server (port 8089)${NC}"
else
    echo -e "${YELLOW}⚠ TTS server not detected on port 8089${NC}"
    echo "  Ada will auto-reconnect when it's available."
fi

# =============================================================================
# Install deps if needed
# =============================================================================

if [ ! -d "$SCRIPT_DIR/venv" ]; then
    echo -e "${CYAN}Creating virtual environment...${NC}"
    python3 -m venv "$SCRIPT_DIR/venv"
fi

source "$SCRIPT_DIR/venv/bin/activate"

# Quick check if deps are installed
if ! python3 -c "import websockets, openai" 2>/dev/null; then
    echo -e "${CYAN}Installing dependencies...${NC}"
    pip install -q -r "$SCRIPT_DIR/requirements.txt"
fi

# =============================================================================
# Launch
# =============================================================================

echo ""
echo -e "${GREEN}Starting Ada...${NC}"
echo -e "  📺 Screen: ws://0.0.0.0:8765"
echo -e "  🎤 Audio:  ws://0.0.0.0:8766"
echo ""

exec python3 "$SCRIPT_DIR/ada_server.py"
