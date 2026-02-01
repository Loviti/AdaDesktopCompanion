"""
Ada Desktop Companion - Tool Executor

Tools Ada can call during conversations:
- Weather lookup
- Web search
- Memory file reading
- Time/date
- Random fun facts
- Screen control

Each tool returns a dict with results and optional screen commands.
"""

import asyncio
import json
import logging
import time
from datetime import datetime, timezone, timedelta
from pathlib import Path
from typing import Any, Optional

import httpx

import config

logger = logging.getLogger("ada.tools")


# =============================================================================
# Tool Definitions (for OpenAI function calling)
# =============================================================================

TOOL_DEFINITIONS = [
    {
        "type": "function",
        "function": {
            "name": "get_weather",
            "description": "Get current weather for a location. Default: Flint, MI.",
            "parameters": {
                "type": "object",
                "properties": {
                    "location": {
                        "type": "string",
                        "description": "City,State,Country code (e.g. 'Flint,MI,US')",
                    }
                },
                "required": [],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "web_search",
            "description": "Search the web for information.",
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "Search query",
                    }
                },
                "required": ["query"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_time",
            "description": "Get current date and time in Chase's timezone (EST).",
            "parameters": {"type": "object", "properties": {}, "required": []},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "read_memory",
            "description": "Read one of Ada's memory files (MEMORY.md, daily notes, etc.)",
            "parameters": {
                "type": "object",
                "properties": {
                    "file": {
                        "type": "string",
                        "description": "File to read: 'memory', 'today', 'yesterday', or a specific date 'YYYY-MM-DD'",
                    }
                },
                "required": ["file"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "set_screen",
            "description": "Control Ada's belly screen directly. Show scenes, emoji, text, etc.",
            "parameters": {
                "type": "object",
                "properties": {
                    "action": {
                        "type": "string",
                        "enum": [
                            "emoji",
                            "text",
                            "code_rain",
                            "ambient",
                            "weather",
                        ],
                        "description": "What to show on the belly screen",
                    },
                    "emoji": {
                        "type": "string",
                        "description": "Emoji to display (for emoji action)",
                    },
                    "text": {
                        "type": "string",
                        "description": "Text to display (for text action)",
                    },
                    "style": {
                        "type": "string",
                        "description": "Text style: bubble, terminal, handwritten, glitch, typewriter",
                    },
                    "charset": {
                        "type": "string",
                        "description": "Code rain charset: katakana, binary, hex, emoji, raccoon",
                    },
                },
                "required": ["action"],
            },
        },
    },
]


# =============================================================================
# Tool Executor
# =============================================================================

class ToolExecutor:
    """Executes tool calls from the LLM and returns results."""

    def __init__(self, screen_engine=None):
        self.screen_engine = screen_engine
        self._http_client: Optional[httpx.AsyncClient] = None

    async def _get_http(self) -> httpx.AsyncClient:
        if self._http_client is None or self._http_client.is_closed:
            self._http_client = httpx.AsyncClient(timeout=15.0)
        return self._http_client

    async def close(self):
        if self._http_client and not self._http_client.is_closed:
            await self._http_client.aclose()

    async def execute(self, tool_name: str, arguments: dict) -> dict:
        """Execute a tool by name with given arguments."""
        logger.info(f"Executing tool: {tool_name}({arguments})")

        handlers = {
            "get_weather": self._get_weather,
            "web_search": self._web_search,
            "get_time": self._get_time,
            "read_memory": self._read_memory,
            "set_screen": self._set_screen,
        }

        handler = handlers.get(tool_name)
        if not handler:
            return {"error": f"Unknown tool: {tool_name}"}

        try:
            result = await handler(**arguments)
            return result
        except Exception as e:
            logger.error(f"Tool {tool_name} failed: {e}")
            return {"error": str(e)}

    # =========================================================================
    # Tool Implementations
    # =========================================================================

    async def _get_weather(self, location: str = "") -> dict:
        """Fetch current weather from OpenWeatherMap."""
        location = location or config.WEATHER_DEFAULT_LOCATION

        if not config.WEATHER_API_KEY:
            # Fallback: return mock data with a note
            return {
                "result": f"Weather API key not configured. Location: {location}",
                "screen_action": "weather_mock",
            }

        client = await self._get_http()
        url = "https://api.openweathermap.org/data/2.5/weather"
        params = {
            "q": location,
            "appid": config.WEATHER_API_KEY,
            "units": config.WEATHER_UNITS,
        }

        resp = await client.get(url, params=params)
        resp.raise_for_status()
        data = resp.json()

        # Parse into our format
        condition_map = {
            "Clear": "sunny",
            "Clouds": "cloudy",
            "Rain": "rain",
            "Drizzle": "rain",
            "Snow": "snow",
            "Thunderstorm": "storm",
            "Mist": "fog",
            "Fog": "fog",
            "Haze": "fog",
        }

        icon_map = {
            "sunny": "☀️",
            "cloudy": "☁️",
            "rain": "🌧️",
            "snow": "❄️",
            "storm": "⛈️",
            "fog": "🌫️",
            "wind": "💨",
        }

        main_condition = data.get("weather", [{}])[0].get("main", "Clouds")
        condition = condition_map.get(main_condition, "cloudy")

        weather_result = {
            "condition": condition,
            "temp_f": round(data["main"]["temp"]),
            "temp_c": round((data["main"]["temp"] - 32) * 5 / 9),
            "humidity": data["main"]["humidity"],
            "description": data["weather"][0]["description"],
            "location": location.split(",")[0],
            "icon": icon_map.get(condition, "🌤️"),
            "wind_speed": round(data.get("wind", {}).get("speed", 0)),
        }

        # Trigger screen update
        if self.screen_engine:
            await self.screen_engine.show_weather(weather_result)

        return {"result": weather_result, "screen_action": "weather_shown"}

    async def _web_search(self, query: str) -> dict:
        """Search the web using Brave Search API."""
        if not config.BRAVE_API_KEY:
            return {
                "result": f"Search API not configured. Query was: {query}",
                "screen_action": None,
            }

        client = await self._get_http()
        url = "https://api.search.brave.com/res/v1/web/search"
        headers = {
            "Accept": "application/json",
            "Accept-Encoding": "gzip",
            "X-Subscription-Token": config.BRAVE_API_KEY,
        }
        params = {"q": query, "count": 5}

        resp = await client.get(url, headers=headers, params=params)
        resp.raise_for_status()
        data = resp.json()

        results = []
        for item in data.get("web", {}).get("results", [])[:5]:
            results.append({
                "title": item.get("title", ""),
                "url": item.get("url", ""),
                "description": item.get("description", ""),
            })

        return {"result": results, "screen_action": None}

    async def _get_time(self) -> dict:
        """Get current time in EST."""
        est = timezone(timedelta(hours=-5))
        now = datetime.now(est)
        result = {
            "time": now.strftime("%I:%M %p"),
            "date": now.strftime("%A, %B %d, %Y"),
            "datetime": now.isoformat(),
            "timezone": "EST",
        }

        # Show time on screen
        if self.screen_engine:
            await self.screen_engine.show_text(
                f"{result['time']}\n{result['date']}",
                style="terminal",
                mood="neutral",
            )

        return {"result": result, "screen_action": "time_shown"}

    async def _read_memory(self, file: str = "memory") -> dict:
        """Read Ada's memory files."""
        est = timezone(timedelta(hours=-5))
        now = datetime.now(est)

        if file == "memory":
            path = config.MEMORY_PATH
        elif file == "today":
            date_str = now.strftime("%Y-%m-%d")
            path = config.CLAWD_DIR / "memory" / f"{date_str}.md"
        elif file == "yesterday":
            yesterday = now - timedelta(days=1)
            date_str = yesterday.strftime("%Y-%m-%d")
            path = config.CLAWD_DIR / "memory" / f"{date_str}.md"
        else:
            # Try as a date
            path = config.CLAWD_DIR / "memory" / f"{file}.md"

        if not path.exists():
            return {"result": f"File not found: {path.name}", "screen_action": None}

        content = path.read_text(encoding="utf-8")
        # Truncate if too long (don't blow up context)
        if len(content) > 3000:
            content = content[:3000] + "\n\n... [truncated]"

        return {"result": content, "screen_action": None}

    async def _set_screen(
        self,
        action: str,
        emoji: str = "🦝",
        text: str = "",
        style: str = "bubble",
        charset: str = "katakana",
    ) -> dict:
        """Direct screen control from LLM."""
        if not self.screen_engine:
            return {"result": "Screen engine not available", "screen_action": None}

        if action == "emoji":
            await self.screen_engine.show_emoji(emoji)
            return {"result": f"Showing emoji: {emoji}", "screen_action": "emoji_shown"}

        elif action == "text":
            await self.screen_engine.show_text(text, style=style)
            return {"result": f"Showing text: {text}", "screen_action": "text_shown"}

        elif action == "code_rain":
            await self.screen_engine.show_code_rain(charset)
            return {"result": f"Code rain: {charset}", "screen_action": "code_rain_shown"}

        elif action == "ambient":
            await self.screen_engine.start_ambient()
            return {"result": "Back to ambient", "screen_action": "ambient_started"}

        elif action == "weather":
            # Trigger weather fetch
            result = await self._get_weather()
            return result

        return {"result": f"Unknown screen action: {action}", "screen_action": None}
