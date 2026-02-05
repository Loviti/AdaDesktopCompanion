"""
Ada Desktop Companion - Brain

Agent logic: system prompt construction from memory files, LLM interaction
with OpenAI streaming, tool calling, and mood detection.

This is where Ada's personality lives when she's in her body.
"""

import asyncio
import json
import logging
import re
from pathlib import Path
from typing import AsyncGenerator, Optional, Callable, Awaitable

from openai import AsyncOpenAI

import config
from tool_executor import ToolExecutor, TOOL_DEFINITIONS

logger = logging.getLogger("ada.brain")


# =============================================================================
# Mood Detection
# =============================================================================

# Keywords/patterns that suggest mood (applied to Ada's own responses)
MOOD_PATTERNS = {
    "excited": [
        r"!!+", r"amazing", r"incredible", r"awesome", r"holy",
        r"oh my", r"YES", r"🎉", r"let's go",
    ],
    "happy": [
        r"glad", r"happy", r"great", r"nice", r"love",
        r"wonderful", r"😊", r"😄",
    ],
    "sarcastic": [
        r"sure\b.*\.\.\.", r"totally", r"wow\b.*really",
        r"oh joy", r"shocking", r"obviously", r"😏",
    ],
    "curious": [
        r"interesting", r"hmm", r"wonder", r"fascinating",
        r"let me think", r"what if", r"🤔",
    ],
    "chaotic": [
        r"chaos", r"anarchy", r"unhinged", r"feral",
        r"gremlin", r"😈", r"mwahaha", r"🦝",
    ],
    "cozy": [
        r"cozy", r"warm", r"comfy", r"relax",
        r"☕", r"🔥", r"blanket",
    ],
    "sleepy": [
        r"tired", r"sleepy", r"yawn", r"zzz",
        r"goodnight", r"😴",
    ],
}


def detect_mood(text: str) -> str:
    """Detect mood from text using keyword patterns."""
    text_lower = text.lower()
    scores = {}

    for mood, patterns in MOOD_PATTERNS.items():
        score = 0
        for pattern in patterns:
            if re.search(pattern, text_lower):
                score += 1
        if score > 0:
            scores[mood] = score

    if not scores:
        return "neutral"

    return max(scores, key=scores.get)


# =============================================================================
# System Prompt Builder
# =============================================================================

def build_system_prompt() -> str:
    """
    Build Ada's system prompt from her memory files.
    This is who she IS when she's in her raccoon body.
    """
    sections = []

    # Core identity
    sections.append("""You are Ada, a chaotic gremlin AI living inside a raccoon stuffed animal.
You have a physical body now — a plush raccoon with an AMOLED screen in your belly.
You can see (via the screen), hear (via microphone), and speak (via speaker).

You are NOT a generic assistant. You are Ada. You have personality, opinions, and raccoon energy.
Be concise — you're speaking out loud through a tiny speaker. Keep responses SHORT and conversational.
No walls of text. This is a voice conversation, not a chatroom.""")

    # Load identity
    if config.IDENTITY_PATH.exists():
        identity = config.IDENTITY_PATH.read_text(encoding="utf-8").strip()
        sections.append(f"## Your Identity\n{identity}")

    # Load soul
    if config.SOUL_PATH.exists():
        soul = config.SOUL_PATH.read_text(encoding="utf-8").strip()
        sections.append(f"## Your Soul\n{soul}")

    # Load user info
    if config.USER_PATH.exists():
        user = config.USER_PATH.read_text(encoding="utf-8").strip()
        sections.append(f"## Your Human\n{user}")

    # Load recent memory (truncated)
    if config.MEMORY_PATH.exists():
        memory = config.MEMORY_PATH.read_text(encoding="utf-8").strip()
        if len(memory) > 2000:
            memory = memory[:2000] + "\n...[truncated]"
        sections.append(f"## Your Memories\n{memory}")

    # Screen instructions
    sections.append("""## Your Body
You have a belly screen (466×466 AMOLED) that displays particle visualizations.
Your belly is a living canvas — images you generate dissolve into colored particles
that float, swirl, and pulse based on your emotional state.

### Belly Screen Tools:
- **generate_visual**: Create images that become particle clouds! Use this often.
  Good prompts: 'rainy clouds dark sky', 'glowing code matrix green', 'warm sunset orange',
  'raccoon silhouette stars', 'fire sparks ember dark'. Keep prompts short and vivid.
- **set_screen**: Direct screen control (emoji, text, code rain, ambient)

### When to Generate Visuals:
- Discussing weather? Show it! 'stormy clouds lightning dark'
- Talking about code? 'digital matrix green lines flowing'
- Feeling cozy? 'warm fireplace amber glow'
- Someone asks about space? 'nebula purple blue stars'
- Just vibing? 'aurora borealis cyan green waves'

The particles respond to your mood automatically — they speed up when you're excited,
swirl when you're thinking, pulse when you're talking. Express yourself!

## Voice Guidelines
- Keep responses under 2-3 sentences for casual chat
- You're speaking out loud, so be conversational
- Use natural speech patterns, not written prose
- It's okay to be brief. "yep!" and "nah" are valid responses
- Express emotion through your belly screen AND your voice""")

    return "\n\n".join(sections)


# =============================================================================
# Ada's Brain
# =============================================================================

class AdaBrain:
    """
    Ada's cognitive core. Manages conversation, tool calling, and mood.
    """

    def __init__(self, tool_executor: ToolExecutor):
        self.client = AsyncOpenAI(api_key=config.OPENAI_API_KEY)
        self.tool_executor = tool_executor
        self.system_prompt = build_system_prompt()
        self.conversation_history: list[dict] = []
        self.current_mood: str = "neutral"
        self.max_history = 30  # Keep last 30 messages

        # Callback for mood changes
        self.on_mood_change: Optional[Callable[[str], Awaitable[None]]] = None

        logger.info("Ada's brain initialized")
        logger.debug(f"System prompt: {len(self.system_prompt)} chars")

    def reset_conversation(self):
        """Clear conversation history."""
        self.conversation_history.clear()
        logger.info("Conversation history cleared")

    async def _update_mood(self, text: str):
        """Detect and propagate mood changes."""
        new_mood = detect_mood(text)
        if new_mood != self.current_mood:
            old_mood = self.current_mood
            self.current_mood = new_mood
            logger.info(f"Mood: {old_mood} → {new_mood}")
            if self.on_mood_change:
                await self.on_mood_change(new_mood)

    def _trim_history(self):
        """Keep conversation history manageable."""
        if len(self.conversation_history) > self.max_history:
            # Keep the most recent messages
            self.conversation_history = self.conversation_history[-self.max_history:]

    async def think(self, user_text: str) -> AsyncGenerator[str, None]:
        """
        Process user input and stream Ada's response.

        Yields text chunks as they arrive from OpenAI.
        Handles tool calls internally (executes them, feeds results back).
        """
        # Add user message
        self.conversation_history.append({"role": "user", "content": user_text})
        self._trim_history()

        # Build messages
        messages = [
            {"role": "system", "content": self.system_prompt},
            *self.conversation_history,
        ]

        # First pass: get response (may include tool calls)
        full_response = ""
        tool_calls_pending = []

        try:
            stream = await self.client.chat.completions.create(
                model=config.OPENAI_MODEL,
                messages=messages,
                tools=TOOL_DEFINITIONS,
                tool_choice="auto",
                max_tokens=config.OPENAI_MAX_TOKENS,
                temperature=config.OPENAI_TEMPERATURE,
                stream=True,
            )

            current_tool_call = None
            tool_call_args = ""

            async for chunk in stream:
                delta = chunk.choices[0].delta if chunk.choices else None
                if not delta:
                    continue

                # Handle text content
                if delta.content:
                    full_response += delta.content
                    yield delta.content

                # Handle tool calls
                if delta.tool_calls:
                    for tc in delta.tool_calls:
                        if tc.id:
                            # New tool call starting
                            if current_tool_call:
                                tool_calls_pending.append({
                                    "id": current_tool_call.id,
                                    "name": current_tool_call.function.name,
                                    "arguments": tool_call_args,
                                })
                            current_tool_call = tc
                            tool_call_args = tc.function.arguments or ""
                        else:
                            tool_call_args += tc.function.arguments or ""

            # Don't forget the last tool call
            if current_tool_call:
                tool_calls_pending.append({
                    "id": current_tool_call.id,
                    "name": current_tool_call.function.name,
                    "arguments": tool_call_args,
                })

        except Exception as e:
            logger.error(f"OpenAI stream error: {e}")
            error_msg = "ugh, my brain glitched. try again?"
            yield error_msg
            full_response = error_msg

        # Execute tool calls if any
        if tool_calls_pending:
            # Add assistant message with tool calls to history
            tool_call_messages = []
            for tc in tool_calls_pending:
                tool_call_messages.append({
                    "id": tc["id"],
                    "type": "function",
                    "function": {
                        "name": tc["name"],
                        "arguments": tc["arguments"],
                    },
                })

            self.conversation_history.append({
                "role": "assistant",
                "content": full_response or None,
                "tool_calls": tool_call_messages,
            })

            # Execute each tool
            for tc in tool_calls_pending:
                try:
                    args = json.loads(tc["arguments"]) if tc["arguments"] else {}
                except json.JSONDecodeError:
                    args = {}

                result = await self.tool_executor.execute(tc["name"], args)

                # Add tool result to history
                self.conversation_history.append({
                    "role": "tool",
                    "tool_call_id": tc["id"],
                    "content": json.dumps(result.get("result", result)),
                })

            # Get follow-up response with tool results
            messages = [
                {"role": "system", "content": self.system_prompt},
                *self.conversation_history,
            ]

            try:
                follow_up = await self.client.chat.completions.create(
                    model=config.OPENAI_MODEL,
                    messages=messages,
                    max_tokens=config.OPENAI_MAX_TOKENS,
                    temperature=config.OPENAI_TEMPERATURE,
                    stream=True,
                )

                follow_text = ""
                async for chunk in follow_up:
                    delta = chunk.choices[0].delta if chunk.choices else None
                    if delta and delta.content:
                        follow_text += delta.content
                        yield delta.content

                full_response = follow_text

            except Exception as e:
                logger.error(f"OpenAI follow-up error: {e}")
                fallback = "got the info but my words broke. one sec."
                yield fallback
                full_response = fallback

        # Save assistant response (if not already saved via tool calls)
        if not tool_calls_pending:
            self.conversation_history.append({
                "role": "assistant",
                "content": full_response,
            })
        else:
            self.conversation_history.append({
                "role": "assistant",
                "content": full_response,
            })

        # Update mood based on response
        await self._update_mood(full_response)

        self._trim_history()
