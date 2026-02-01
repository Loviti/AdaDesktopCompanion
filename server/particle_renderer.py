"""
Ada Desktop Companion - Server-Side Particle Renderer

Renders particle system entirely on the server and streams JPEG frames
to the ESP32 display. The ESP32 is just a dumb JPEG display.

This gives us:
- Perfect circle rendering
- No tearing
- All computation on powerful server hardware
- Web GUI preview = exact display output
- Change effects without reflashing
"""

import io
import math
import random
import time
from dataclasses import dataclass, field
from typing import Optional

from PIL import Image, ImageDraw, ImageFilter

# ============================================
# Particle
# ============================================

@dataclass
class Particle:
    x: float = 0.0
    y: float = 0.0
    home_x: float = 0.0
    home_y: float = 0.0
    target_home_x: float = 0.0
    target_home_y: float = 0.0
    r: int = 0
    g: int = 200
    b: int = 200
    target_r: int = 0
    target_g: int = 200
    target_b: int = 200
    opacity: float = 0.0
    target_opacity: float = 1.0
    morphing: bool = False
    angle_xy: float = 0.0
    angle_xz: float = 0.0
    angular_speed_xy: float = 0.0
    angular_speed_xz: float = 0.0
    orbit_radius: float = 0.0
    phase: float = 0.0


# ============================================
# Particle Config
# ============================================

@dataclass
class ParticleConfig:
    particle_count: int = 350
    particle_size: float = 4.0
    particle_speed: float = 1.0
    dispersion: float = 30.0
    opacity: float = 1.0
    shape: str = "circle"       # circle, square, star
    animation: str = "float"    # float, drift, swirl_inward, pulse_outward
    pulse_speed: float = 1.0
    rotation_speed: float = 0.0
    bg_color: str = "#000000"
    link_count: int = 0
    link_opacity: float = 0.2
    color_mode: str = "original"

    def to_dict(self) -> dict:
        return {
            "particle_count": self.particle_count,
            "particle_size": self.particle_size,
            "particle_speed": self.particle_speed,
            "dispersion": self.dispersion,
            "opacity": self.opacity,
            "shape": self.shape,
            "animation": self.animation,
            "pulse_speed": self.pulse_speed,
            "rotation_speed": self.rotation_speed,
            "bg_color": self.bg_color,
            "link_count": self.link_count,
            "link_opacity": self.link_opacity,
            "color_mode": self.color_mode,
        }

    def update_from_dict(self, d: dict):
        for key, val in d.items():
            if hasattr(self, key):
                setattr(self, key, val)


# ============================================
# Particle Renderer
# ============================================

class ParticleRenderer:
    """Server-side particle system that renders to JPEG frames."""

    def __init__(self, width: int = 466, height: int = 466):
        self.width = width
        self.height = height
        self.particles: list[Particle] = []
        self.config = ParticleConfig()
        self.target_config = ParticleConfig()

        # Animation state
        self.global_rotation: float = 0.0
        self.pulse_phase: float = 0.0
        self.has_image: bool = False
        self.clearing: bool = False

        # Rendering options
        self.jpeg_quality: int = 80
        self.antialias: bool = True  # Slight blur for smoother particles
        self.glow: bool = True       # Add subtle glow effect

    def create_startup_particles(self):
        """Create initial particles for startup animation."""
        cx, cy = self.width / 2, self.height / 2
        count = self.config.particle_count

        self.particles = []
        for i in range(count):
            p = Particle(
                x=cx,
                y=cy,
                home_x=cx + random.uniform(-self.width / 3, self.width / 3),
                home_y=cy + random.uniform(-self.height / 3, self.height / 3),
                r=0,
                g=random.randint(180, 255),
                b=random.randint(180, 255),
                opacity=0.0,
                target_opacity=0.9,
                angle_xy=random.uniform(0, math.tau),
                angle_xz=random.uniform(0, math.tau),
                angular_speed_xy=0.5 + random.random(),
                angular_speed_xz=0.3 + random.random() * 0.67,
                orbit_radius=random.uniform(0, self.config.dispersion),
                phase=random.uniform(0, math.tau),
            )
            p.target_home_x = p.home_x
            p.target_home_y = p.home_y
            p.target_r = p.r
            p.target_g = p.g
            p.target_b = p.b
            self.particles.append(p)

        self.has_image = False
        self.clearing = False

    def create_from_image(self, rgb_data: bytes, img_w: int, img_h: int):
        """Create particles from RGB image data."""
        target_count = min(self.target_config.particle_count, 800)
        brightness_threshold = 15

        # Collect non-black pixels
        valid_pixels = []
        for i in range(img_w * img_h):
            idx = i * 3
            if idx + 2 >= len(rgb_data):
                break
            r, g, b = rgb_data[idx], rgb_data[idx + 1], rgb_data[idx + 2]
            if r + g + b > brightness_threshold:
                px = i % img_w
                py = i // img_w
                valid_pixels.append((px, py, r, g, b))

        if not valid_pixels:
            return

        # Scale to screen
        scale_x = self.width * 0.85 / img_w
        scale_y = self.height * 0.85 / img_h
        scale = min(scale_x, scale_y)
        offset_x = (self.width - img_w * scale) / 2
        offset_y = (self.height - img_h * scale) / 2

        # Sample pixels
        stride = max(1, len(valid_pixels) / target_count)
        new_particles = []

        accumulator = 0.0
        for px, py, r, g, b in valid_pixels:
            accumulator += 1.0
            if accumulator >= stride:
                accumulator -= stride
                screen_x = offset_x + px * scale
                screen_y = offset_y + py * scale

                p = Particle(
                    x=self.width / 2 if not self.has_image else screen_x,
                    y=self.height / 2 if not self.has_image else screen_y,
                    home_x=screen_x,
                    home_y=screen_y,
                    target_home_x=screen_x,
                    target_home_y=screen_y,
                    r=r, g=g, b=b,
                    target_r=r, target_g=g, target_b=b,
                    opacity=0.0,
                    target_opacity=1.0,
                    angle_xy=random.uniform(0, math.tau),
                    angle_xz=random.uniform(0, math.tau),
                    angular_speed_xy=0.5 + random.random(),
                    angular_speed_xz=0.3 + random.random() * 0.67,
                    orbit_radius=random.uniform(0, self.config.dispersion),
                    phase=random.uniform(0, math.tau),
                )
                new_particles.append(p)

                if len(new_particles) >= target_count:
                    break

        self.particles = new_particles
        self.has_image = True
        self.clearing = False

    def update_config(self, config_dict: dict):
        """Update target config from dict."""
        self.target_config.update_from_dict(config_dict)

    def clear(self):
        """Start fading all particles out."""
        self.clearing = True

    def update(self, dt: float):
        """Update particle physics."""
        # Lerp config
        t = min(1.0, 3.0 * dt)
        self.config.particle_size += (self.target_config.particle_size - self.config.particle_size) * t
        self.config.particle_speed += (self.target_config.particle_speed - self.config.particle_speed) * t
        self.config.dispersion += (self.target_config.dispersion - self.config.dispersion) * t
        self.config.opacity += (self.target_config.opacity - self.config.opacity) * t
        self.config.pulse_speed += (self.target_config.pulse_speed - self.config.pulse_speed) * t
        self.config.rotation_speed += (self.target_config.rotation_speed - self.config.rotation_speed) * t
        self.config.link_opacity += (self.target_config.link_opacity - self.config.link_opacity) * t

        # Snap discrete values
        self.config.animation = self.target_config.animation
        self.config.shape = self.target_config.shape
        self.config.bg_color = self.target_config.bg_color
        self.config.particle_count = self.target_config.particle_count
        self.config.link_count = self.target_config.link_count

        # Global state
        self.global_rotation += self.config.rotation_speed * dt
        self.pulse_phase += self.config.pulse_speed * dt

        cx, cy = self.width / 2, self.height / 2
        effective_count = min(len(self.particles), self.config.particle_count)

        for i, p in enumerate(self.particles):
            # Fade out excess particles
            if i >= effective_count:
                p.opacity = max(0.0, p.opacity - 2.0 * dt)
                continue

            # Morphing
            if p.morphing:
                ms = 2.0 * dt
                p.home_x += (p.target_home_x - p.home_x) * ms
                p.home_y += (p.target_home_y - p.home_y) * ms
                p.r = int(p.r + (p.target_r - p.r) * ms)
                p.g = int(p.g + (p.target_g - p.g) * ms)
                p.b = int(p.b + (p.target_b - p.b) * ms)
                if abs(p.home_x - p.target_home_x) + abs(p.home_y - p.target_home_y) < 1:
                    p.morphing = False

            # Opacity
            target_op = 0.0 if self.clearing else p.target_opacity * self.config.opacity
            if p.opacity < target_op:
                p.opacity = min(target_op, p.opacity + 2.0 * dt)
            elif p.opacity > target_op:
                p.opacity = max(target_op, p.opacity - 2.0 * dt)

            # Orbit angles
            p.angle_xy += p.angular_speed_xy * self.config.particle_speed * dt
            p.angle_xz += p.angular_speed_xz * self.config.particle_speed * dt

            # Target orbit radius
            target_radius = self.config.dispersion * (0.5 + 0.5 * math.sin(p.phase + self.pulse_phase))
            p.orbit_radius += (target_radius - p.orbit_radius) * 2.0 * dt

            # Animation-specific position
            anim_x, anim_y = 0.0, 0.0

            if self.config.animation == "float":
                anim_x = math.cos(p.angle_xy) * p.orbit_radius
                anim_y = math.sin(p.angle_xz) * p.orbit_radius

            elif self.config.animation == "drift":
                anim_x = math.cos(p.angle_xy * 0.3) * p.orbit_radius * 0.5
                anim_y = math.sin(p.angle_xz * 0.3) * p.orbit_radius * 0.5

            elif self.config.animation == "swirl_inward":
                dx = p.home_x - cx
                dy = p.home_y - cy
                angle = math.atan2(dy, dx) + p.angle_xy
                pull = 0.7 + 0.3 * math.sin(self.pulse_phase + p.phase)
                anim_x = math.cos(angle) * p.orbit_radius * pull - dx * 0.1 * math.sin(self.pulse_phase)
                anim_y = math.sin(angle) * p.orbit_radius * pull - dy * 0.1 * math.sin(self.pulse_phase)

            elif self.config.animation == "pulse_outward":
                dx = p.home_x - cx
                dy = p.home_y - cy
                dist = math.sqrt(dx * dx + dy * dy) + 1.0
                pulse_wave = math.sin(self.pulse_phase - dist * 0.02)
                push = pulse_wave * self.config.dispersion * 0.3
                anim_x = math.cos(p.angle_xy) * p.orbit_radius + (dx / dist) * push
                anim_y = math.sin(p.angle_xz) * p.orbit_radius + (dy / dist) * push

            # Global rotation
            if abs(self.config.rotation_speed) > 0.01:
                rad = math.radians(self.global_rotation)
                cos_r, sin_r = math.cos(rad), math.sin(rad)
                anim_x, anim_y = anim_x * cos_r - anim_y * sin_r, anim_x * sin_r + anim_y * cos_r

            # Smooth position update
            target_x = p.home_x + anim_x
            target_y = p.home_y + anim_y
            lerp = min(1.0, 4.0 * dt)
            p.x += (target_x - p.x) * lerp
            p.y += (target_y - p.y) * lerp

        # Remove fully faded particles
        self.particles = [p for p in self.particles if p.opacity > 0.01 or p.morphing or not self.clearing]

    def render_frame(self) -> bytes:
        """Render current state to JPEG bytes."""
        # Parse background color
        bg = self.config.bg_color
        if bg.startswith("#") and len(bg) == 7:
            bg_r = int(bg[1:3], 16)
            bg_g = int(bg[3:5], 16)
            bg_b = int(bg[5:7], 16)
        else:
            bg_r, bg_g, bg_b = 0, 0, 0

        # Create image
        img = Image.new("RGB", (self.width, self.height), (bg_r, bg_g, bg_b))
        draw = ImageDraw.Draw(img)

        size = max(1, int(self.config.particle_size + 0.5))
        effective = min(len(self.particles), self.config.particle_count)

        # Draw links first (behind particles)
        if self.config.link_count > 0 and self.config.link_opacity > 0.01:
            max_dist = self.config.dispersion * 2.0
            links_drawn = 0
            max_links = self.config.link_count

            for _ in range(max_links * 3):
                if links_drawn >= max_links:
                    break
                a = random.randint(0, effective - 1) if effective > 0 else 0
                b = random.randint(0, effective - 1) if effective > 0 else 0
                if a == b or a >= len(self.particles) or b >= len(self.particles):
                    continue

                pa, pb = self.particles[a], self.particles[b]
                dx = pa.x - pb.x
                dy = pa.y - pb.y
                dist = math.sqrt(dx * dx + dy * dy)

                if dist < max_dist and dist > 2:
                    alpha = int((1 - dist / max_dist) * self.config.link_opacity * 255)
                    alpha = max(0, min(255, alpha))
                    draw.line(
                        [(int(pa.x), int(pa.y)), (int(pb.x), int(pb.y))],
                        fill=(alpha // 2, alpha, alpha),
                        width=1,
                    )
                    links_drawn += 1

        # Draw particles
        for i in range(effective):
            if i >= len(self.particles):
                break
            p = self.particles[i]

            if p.opacity < 0.05:
                continue

            sx, sy = int(p.x + 0.5), int(p.y + 0.5)
            if sx < -size or sx >= self.width + size or sy < -size or sy >= self.height + size:
                continue

            dr = int(p.r * p.opacity)
            dg = int(p.g * p.opacity)
            db = int(p.b * p.opacity)
            color = (min(255, dr), min(255, dg), min(255, db))

            if self.config.shape == "circle":
                draw.ellipse(
                    [sx - size, sy - size, sx + size, sy + size],
                    fill=color,
                )
            elif self.config.shape == "square":
                draw.rectangle(
                    [sx - size, sy - size, sx + size, sy + size],
                    fill=color,
                )
            elif self.config.shape == "star":
                # Simple 4-point star
                pts = [
                    (sx, sy - size),
                    (sx + size // 3, sy - size // 3),
                    (sx + size, sy),
                    (sx + size // 3, sy + size // 3),
                    (sx, sy + size),
                    (sx - size // 3, sy + size // 3),
                    (sx - size, sy),
                    (sx - size // 3, sy - size // 3),
                ]
                draw.polygon(pts, fill=color)

        # Optional: subtle glow effect (slight blur on a copy, then composite)
        if self.glow and effective > 0:
            try:
                glow_layer = img.filter(ImageFilter.GaussianBlur(radius=2))
                from PIL import ImageChops
                img = ImageChops.add(img, glow_layer, scale=2, offset=0)
            except Exception:
                pass

        # Encode to JPEG
        buf = io.BytesIO()
        img.save(buf, format="JPEG", quality=self.jpeg_quality)
        return buf.getvalue()

    def get_particle_count(self) -> int:
        return len(self.particles)
