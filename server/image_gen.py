"""
Ada Desktop Companion - Image Generation

Uses SD-Turbo (stabilityai/sd-turbo) for single-step diffusion image generation.
Generates small images (128x128) that get decomposed into particles on the ESP32 belly screen.

This is how Ada visualizes her thoughts — turning prompts into living particle clouds.
"""

import asyncio
import logging
from typing import Optional

import torch

import config

logger = logging.getLogger("ada.image_gen")

# Module-level model cache
_pipeline = None
_pipeline_lock = asyncio.Lock()


async def initialize():
    """
    Load the image generation model into VRAM at startup.
    Call this once during server init — model stays resident.
    """
    global _pipeline

    async with _pipeline_lock:
        if _pipeline is not None:
            logger.info("Image gen model already loaded")
            return

        logger.info(f"Loading image gen model: {config.IMAGE_GEN_MODEL}")
        logger.info(f"Target resolution: {config.IMAGE_GEN_WIDTH}x{config.IMAGE_GEN_HEIGHT}")

        # Run the heavy model load in a thread to not block the event loop
        _pipeline = await asyncio.get_event_loop().run_in_executor(None, _load_pipeline)

        if _pipeline is not None:
            logger.info("Image gen model loaded and ready in VRAM")
        else:
            logger.error("Failed to load image gen model")


def _load_pipeline():
    """
    Synchronous model loading. Tries SD-Turbo first, falls back to smaller models.
    """
    from diffusers import AutoPipelineForText2Image

    models_to_try = [
        config.IMAGE_GEN_MODEL,
        "segmind/small-sd",  # Fallback: smaller model
    ]

    for model_id in models_to_try:
        try:
            logger.info(f"Attempting to load: {model_id}")

            pipe = AutoPipelineForText2Image.from_pretrained(
                model_id,
                torch_dtype=torch.float16,
                variant="fp16",
            )
            pipe = pipe.to("cuda")

            # Optimizations for speed
            pipe.set_progress_bar_config(disable=True)

            # Try to enable memory-efficient attention
            try:
                pipe.enable_xformers_memory_efficient_attention()
                logger.info("xformers memory-efficient attention enabled")
            except Exception:
                logger.info("xformers not available, using default attention")

            # Warmup with a tiny generation
            logger.info("Warming up pipeline...")
            _ = pipe(
                prompt="test",
                num_inference_steps=config.IMAGE_GEN_STEPS,
                guidance_scale=config.IMAGE_GEN_GUIDANCE,
                width=64,
                height=64,
            )

            logger.info(f"Successfully loaded: {model_id}")
            return pipe

        except Exception as e:
            logger.warning(f"Failed to load {model_id}: {e}")
            continue

    logger.error("All image gen models failed to load")
    return None


async def generate_image(
    prompt: str,
    width: int = 0,
    height: int = 0,
) -> Optional[bytes]:
    """
    Generate an image from a text prompt and return raw RGB pixel data.

    Args:
        prompt: Text description of the image to generate.
        width: Image width in pixels (default from config).
        height: Image height in pixels (default from config).

    Returns:
        Raw RGB bytes (width * height * 3) or None on failure.
    """
    global _pipeline

    if _pipeline is None:
        logger.error("Image gen pipeline not initialized — call initialize() first")
        return None

    width = width or config.IMAGE_GEN_WIDTH
    height = height or config.IMAGE_GEN_HEIGHT

    logger.info(f"Generating image: '{prompt}' ({width}x{height})")

    try:
        # Run generation in executor to not block async loop
        rgb_bytes = await asyncio.get_event_loop().run_in_executor(
            None,
            _generate_sync,
            prompt,
            width,
            height,
        )

        if rgb_bytes:
            logger.info(f"Image generated: {len(rgb_bytes)} bytes ({width}x{height})")
        return rgb_bytes

    except Exception as e:
        logger.error(f"Image generation failed: {e}")
        return None


def _generate_sync(prompt: str, width: int, height: int) -> Optional[bytes]:
    """
    Synchronous image generation. Called from executor.
    """
    global _pipeline

    if _pipeline is None:
        return None

    try:
        # SD-Turbo: 1 step, 0 guidance
        result = _pipeline(
            prompt=prompt,
            num_inference_steps=config.IMAGE_GEN_STEPS,
            guidance_scale=config.IMAGE_GEN_GUIDANCE,
            width=width,
            height=height,
        )

        image = result.images[0]

        # Ensure RGB mode
        if image.mode != "RGB":
            image = image.convert("RGB")

        # Return raw RGB pixel bytes
        return image.tobytes()

    except torch.cuda.OutOfMemoryError:
        logger.error("CUDA out of memory during generation")
        torch.cuda.empty_cache()
        return None
    except Exception as e:
        logger.error(f"Generation error: {e}")
        return None


async def is_ready() -> bool:
    """Check if the image gen pipeline is loaded and ready."""
    return _pipeline is not None


async def unload():
    """Unload the model from VRAM."""
    global _pipeline

    async with _pipeline_lock:
        if _pipeline is not None:
            del _pipeline
            _pipeline = None
            torch.cuda.empty_cache()
            logger.info("Image gen model unloaded")
