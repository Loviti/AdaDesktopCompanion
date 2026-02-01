"""
Ada Desktop Companion - Image Generation (Graceful Degradation)

Tries to use SD-Turbo if torch/diffusers are available.
Falls back to test_patterns if not — server runs either way.
"""

import asyncio
import logging
from typing import Optional

import config

logger = logging.getLogger("ada.image_gen")

_pipeline = None
_pipeline_lock = asyncio.Lock()
_torch_available = False

try:
    import torch
    _torch_available = True
except ImportError:
    logger.info("torch not available — image generation disabled, using test patterns")


async def initialize():
    """Load image gen model if torch/diffusers available."""
    global _pipeline

    if not _torch_available:
        logger.info("Skipping image gen init (torch not installed)")
        return

    async with _pipeline_lock:
        if _pipeline is not None:
            return

        logger.info(f"Loading image gen model: {config.IMAGE_GEN_MODEL}")
        try:
            _pipeline = await asyncio.get_event_loop().run_in_executor(None, _load_pipeline)
            if _pipeline:
                logger.info("Image gen model loaded")
            else:
                logger.warning("Image gen model failed to load")
        except Exception as e:
            logger.warning(f"Image gen init failed: {e}")


def _load_pipeline():
    """Synchronous model loading."""
    if not _torch_available:
        return None
    try:
        from diffusers import AutoPipelineForText2Image
        pipe = AutoPipelineForText2Image.from_pretrained(
            config.IMAGE_GEN_MODEL,
            torch_dtype=torch.float16,
            variant="fp16",
        )
        pipe = pipe.to("cuda")
        pipe.set_progress_bar_config(disable=True)
        try:
            pipe.enable_xformers_memory_efficient_attention()
        except Exception:
            pass
        # Warmup
        _ = pipe(prompt="test", num_inference_steps=1, guidance_scale=0.0, width=64, height=64)
        return pipe
    except Exception as e:
        logger.warning(f"Pipeline load failed: {e}")
        return None


async def generate_image(prompt: str, width: int = 0, height: int = 0) -> Optional[bytes]:
    """Generate image or return None."""
    global _pipeline
    if _pipeline is None:
        return None

    width = width or config.IMAGE_GEN_WIDTH
    height = height or config.IMAGE_GEN_HEIGHT

    try:
        return await asyncio.get_event_loop().run_in_executor(
            None, _generate_sync, prompt, width, height
        )
    except Exception as e:
        logger.error(f"Image generation failed: {e}")
        return None


def _generate_sync(prompt: str, width: int, height: int) -> Optional[bytes]:
    if _pipeline is None:
        return None
    try:
        result = _pipeline(
            prompt=prompt,
            num_inference_steps=config.IMAGE_GEN_STEPS,
            guidance_scale=config.IMAGE_GEN_GUIDANCE,
            width=width, height=height,
        )
        image = result.images[0]
        if image.mode != "RGB":
            image = image.convert("RGB")
        return image.tobytes()
    except Exception as e:
        logger.error(f"Generation error: {e}")
        if _torch_available:
            torch.cuda.empty_cache()
        return None


async def is_ready() -> bool:
    return _pipeline is not None


async def unload():
    global _pipeline
    async with _pipeline_lock:
        if _pipeline is not None:
            del _pipeline
            _pipeline = None
            if _torch_available:
                torch.cuda.empty_cache()
            logger.info("Image gen model unloaded")
