"""
Ada Desktop Companion - Test Pattern Generator

Generates 128x128 RGB test images WITHOUT needing SD-Turbo.
Used for immediate particle display on ESP32 connect.
"""

import math
import struct
from typing import Callable


def generate_raccoon_silhouette(width: int = 128, height: int = 128) -> bytes:
    """
    Generate a stylized raccoon face silhouette with teal/cyan glow.
    Returns raw RGB bytes (width * height * 3).
    """
    pixels = bytearray(width * height * 3)
    cx, cy = width // 2, height // 2

    for y in range(height):
        for x in range(width):
            idx = (y * width + x) * 3
            dx = x - cx
            dy = y - cy
            dist = math.sqrt(dx * dx + dy * dy)

            r, g, b = 0, 0, 0

            # Main face circle
            face_radius = 38
            if dist < face_radius:
                t = 1.0 - (dist / face_radius)
                r = int(20 * t)
                g = int(200 * t * t)
                b = int(180 * t)

            # Left ear
            ear_lx, ear_ly = cx - 28, cy - 35
            ear_dist_l = math.sqrt((x - ear_lx) ** 2 + (y - ear_ly) ** 2)
            if ear_dist_l < 16:
                t = 1.0 - (ear_dist_l / 16)
                r = max(r, int(30 * t))
                g = max(g, int(220 * t))
                b = max(b, int(200 * t))

            # Right ear
            ear_rx, ear_ry = cx + 28, cy - 35
            ear_dist_r = math.sqrt((x - ear_rx) ** 2 + (y - ear_ry) ** 2)
            if ear_dist_r < 16:
                t = 1.0 - (ear_dist_r / 16)
                r = max(r, int(30 * t))
                g = max(g, int(220 * t))
                b = max(b, int(200 * t))

            # Eye mask (dark band across eyes)
            mask_y_center = cy - 8
            if abs(y - mask_y_center) < 10 and abs(dx) < 34:
                mask_t = 1.0 - abs(y - mask_y_center) / 10
                mask_x_t = 1.0 - abs(dx) / 34
                mask_intensity = mask_t * mask_x_t
                r = int(r * (1 - mask_intensity * 0.7))
                g = int(g * (1 - mask_intensity * 0.5))
                b = int(b * (1 - mask_intensity * 0.3))

            # Eyes (bright cyan dots)
            for eye_x in [cx - 14, cx + 14]:
                eye_y = cy - 8
                eye_dist = math.sqrt((x - eye_x) ** 2 + (y - eye_y) ** 2)
                if eye_dist < 5:
                    t = 1.0 - (eye_dist / 5)
                    r = max(r, int(100 * t))
                    g = max(g, int(255 * t))
                    b = max(b, int(255 * t))

            # Nose (small bright spot)
            nose_dist = math.sqrt((x - cx) ** 2 + (y - (cy + 8)) ** 2)
            if nose_dist < 4:
                t = 1.0 - (nose_dist / 4)
                r = max(r, int(200 * t))
                g = max(g, int(150 * t))
                b = max(b, int(100 * t))

            # Background glow
            if r == 0 and g == 0 and b == 0:
                bg_t = max(0, 1.0 - dist / 64)
                r = int(5 * bg_t)
                g = int(30 * bg_t * bg_t)
                b = int(40 * bg_t * bg_t)

            pixels[idx] = min(255, r)
            pixels[idx + 1] = min(255, g)
            pixels[idx + 2] = min(255, b)

    return bytes(pixels)


def generate_gradient_orb(width: int = 128, height: int = 128) -> bytes:
    """
    Generate a glowing teal/purple orb with particle-like noise.
    Returns raw RGB bytes.
    """
    pixels = bytearray(width * height * 3)
    cx, cy = width // 2, height // 2

    # Simple LCG for deterministic "random" noise
    seed = 42

    for y in range(height):
        for x in range(width):
            idx = (y * width + x) * 3
            dx = (x - cx) / cx
            dy = (y - cy) / cy
            dist = math.sqrt(dx * dx + dy * dy)

            # Pseudo-random noise
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
            noise = (seed % 100) / 100.0

            if dist < 1.0:
                t = 1.0 - dist
                # Teal core fading to purple edge
                r = int((20 + 80 * (1 - t)) * t + noise * 15)
                g = int((220 * t * t) + noise * 10)
                b = int((200 * t + 55 * (1 - t) * t) + noise * 12)

                # Add sparkle points
                if noise > 0.92 and t > 0.3:
                    sparkle = (noise - 0.92) / 0.08
                    r = min(255, r + int(200 * sparkle))
                    g = min(255, g + int(255 * sparkle))
                    b = min(255, b + int(255 * sparkle))
            else:
                r, g, b = 0, 0, 0

            pixels[idx] = min(255, max(0, r))
            pixels[idx + 1] = min(255, max(0, g))
            pixels[idx + 2] = min(255, max(0, b))

    return bytes(pixels)


def generate_aurora(width: int = 128, height: int = 128) -> bytes:
    """
    Generate an aurora borealis pattern.
    Returns raw RGB bytes.
    """
    pixels = bytearray(width * height * 3)

    for y in range(height):
        for x in range(width):
            idx = (y * width + x) * 3
            nx = x / width
            ny = y / height

            # Layered sine waves for aurora bands
            wave1 = math.sin(nx * 6.0 + ny * 2.0) * 0.5 + 0.5
            wave2 = math.sin(nx * 3.0 - ny * 4.0 + 1.5) * 0.5 + 0.5
            wave3 = math.sin(nx * 8.0 + ny * 1.0 + 3.0) * 0.5 + 0.5

            # Vertical fade (aurora at top)
            v_fade = max(0, 1.0 - ny * 1.2)
            v_fade = v_fade * v_fade

            # Combine
            intensity = (wave1 * 0.5 + wave2 * 0.3 + wave3 * 0.2) * v_fade

            # Teal to green to purple color ramp
            r = int(40 * wave2 * intensity * 255)
            g = int((0.5 * wave1 + 0.3 * wave3) * intensity * 255)
            b = int((0.3 * wave1 + 0.5 * wave2) * intensity * 255)

            pixels[idx] = min(255, max(0, r))
            pixels[idx + 1] = min(255, max(0, g))
            pixels[idx + 2] = min(255, max(0, b))

    return bytes(pixels)


# Default pattern to send on ESP32 connect
def get_default_image(width: int = 128, height: int = 128) -> bytes:
    """Get the default startup image for ESP32."""
    return generate_raccoon_silhouette(width, height)


# All available test patterns
PATTERNS: dict[str, Callable] = {
    "raccoon": generate_raccoon_silhouette,
    "orb": generate_gradient_orb,
    "aurora": generate_aurora,
}


if __name__ == "__main__":
    # Quick test - generate all patterns and print sizes
    for name, fn in PATTERNS.items():
        data = fn()
        print(f"{name}: {len(data)} bytes ({len(data) // 3} pixels)")
