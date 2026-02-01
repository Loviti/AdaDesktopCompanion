#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "config.h"

// ============================================
// Particle Structure
// ============================================

struct Particle {
    // Current position (screen space)
    float x, y, z;

    // Home position from image (what particle returns to)
    float homeX, homeY, homeZ;

    // Target home (for morphing to new image)
    float targetHomeX, targetHomeY, targetHomeZ;
    bool morphing;

    // Orbit angles for animation
    float angleXY;
    float angleXZ;
    float angularSpeedXY;
    float angularSpeedXZ;
    float orbitRadius;

    // Color from source image
    uint8_t r, g, b;
    uint8_t targetR, targetG, targetB;

    // Per-particle opacity (for fade in/out)
    float opacity;
    float targetOpacity;

    // Random offset for variation
    float phase;
};

// ============================================
// Particle Config (mirrors server JSON)
// ============================================

struct ParticleConfig {
    int particle_count;
    float particle_size;
    float particle_speed;
    float dispersion;
    float opacity;
    // shape: 0=circle, 1=square, 2=star
    uint8_t shape;
    // animation: 0=float, 1=drift, 2=swirl_inward, 3=pulse_outward
    uint8_t animation;
    float pulse_speed;
    float rotation_speed;
    uint16_t bg_color;       // RGB565
    // color_mode: 0=original, 1=monochrome
    uint8_t color_mode;
    int link_count;
    float link_opacity;
};

// Animation type enum for clarity
enum AnimationType : uint8_t {
    ANIM_FLOAT = 0,
    ANIM_DRIFT = 1,
    ANIM_SWIRL_INWARD = 2,
    ANIM_PULSE_OUTWARD = 3,
};

// Shape enum
enum ShapeType : uint8_t {
    SHAPE_CIRCLE = 0,
    SHAPE_SQUARE = 1,
    SHAPE_STAR = 2,
};

// ============================================
// Particle System
// ============================================

class ParticleSystem {
public:
    ParticleSystem() {
        particles = nullptr;
        activeCount = 0;
        globalRotation = 0.0f;
        pulsePhase = 0.0f;
        _clearing = false;
        _startupPhase = 0.0f;
        _startupActive = false;
        _hasImage = false;

        // Default config
        config.particle_count = DEFAULT_PARTICLE_COUNT;
        config.particle_size = DEFAULT_PARTICLE_SIZE;
        config.particle_speed = DEFAULT_PARTICLE_SPEED;
        config.dispersion = DEFAULT_DISPERSION;
        config.opacity = DEFAULT_OPACITY;
        config.shape = SHAPE_CIRCLE;
        config.animation = ANIM_FLOAT;
        config.pulse_speed = DEFAULT_PULSE_SPEED;
        config.rotation_speed = DEFAULT_ROTATION_SPEED;
        config.bg_color = 0x0000;  // Black
        config.color_mode = 0;
        config.link_count = 0;
        config.link_opacity = 0.2f;

        targetConfig = config;
    }

    /**
     * Initialize — allocate particle array in PSRAM.
     * Must be called once after PSRAM is available.
     */
    bool init() {
        particles = (Particle*)ps_malloc(sizeof(Particle) * MAX_PARTICLES);
        if (!particles) {
            USBSerial.println("ERROR: Failed to allocate particle array in PSRAM");
            return false;
        }
        memset(particles, 0, sizeof(Particle) * MAX_PARTICLES);

        USBSerial.printf("Particle system: %d bytes allocated in PSRAM\n",
                         (int)(sizeof(Particle) * MAX_PARTICLES));
        return true;
    }

    /**
     * Create particles from raw RGB image data.
     * Samples non-black pixels, assigns colors, maps to screen space.
     */
    void createFromImage(uint8_t* rgbData, int imgW, int imgH) {
        if (!particles || !rgbData) return;

        // First pass: count non-black pixels and build candidate list
        // We'll sample from non-black pixels to fill particle_count particles
        int totalPixels = imgW * imgH;
        int targetCount = min(targetConfig.particle_count, MAX_PARTICLES);

        // Collect non-black pixel indices
        // Use a simple brightness threshold to skip near-black pixels
        const int BRIGHTNESS_THRESHOLD = 15;

        // Count valid pixels first
        int validCount = 0;
        for (int i = 0; i < totalPixels; i++) {
            int idx = i * 3;
            int brightness = (int)rgbData[idx] + rgbData[idx + 1] + rgbData[idx + 2];
            if (brightness > BRIGHTNESS_THRESHOLD) {
                validCount++;
            }
        }

        if (validCount == 0) {
            // All black image — create some particles at center with dim colors
            for (int i = 0; i < min(targetCount, 100); i++) {
                _initParticle(i,
                    SCREEN_WIDTH / 2.0f + random(-50, 50),
                    SCREEN_HEIGHT / 2.0f + random(-50, 50),
                    30, 30, 40);  // dim blue-ish
            }
            activeCount = min(targetCount, 100);
            _hasImage = true;
            return;
        }

        // Calculate sampling stride
        float stride = (float)validCount / (float)targetCount;
        if (stride < 1.0f) stride = 1.0f;

        // Scale factors: image coords → screen coords
        // Center the image on screen with some padding
        float scaleX = (float)SCREEN_WIDTH * 0.85f / (float)imgW;
        float scaleY = (float)SCREEN_HEIGHT * 0.85f / (float)imgH;
        float scale = min(scaleX, scaleY);
        float offsetX = (SCREEN_WIDTH - imgW * scale) / 2.0f;
        float offsetY = (SCREEN_HEIGHT - imgH * scale) / 2.0f;

        int particleIdx = 0;
        float accumulator = 0.0f;
        int validIdx = 0;

        for (int i = 0; i < totalPixels && particleIdx < targetCount; i++) {
            int idx = i * 3;
            int brightness = (int)rgbData[idx] + rgbData[idx + 1] + rgbData[idx + 2];

            if (brightness <= BRIGHTNESS_THRESHOLD) continue;

            accumulator += 1.0f;
            if (accumulator >= stride) {
                accumulator -= stride;

                // Pixel position in image space
                int px = i % imgW;
                int py = i / imgW;

                // Map to screen space
                float screenX = offsetX + px * scale;
                float screenY = offsetY + py * scale;

                // Set up particle — morph from current position if already active
                Particle& p = particles[particleIdx];

                if (_hasImage && particleIdx < activeCount) {
                    // Existing particle: morph to new position
                    p.targetHomeX = screenX;
                    p.targetHomeY = screenY;
                    p.targetHomeZ = 0.0f;
                    p.targetR = rgbData[idx];
                    p.targetG = rgbData[idx + 1];
                    p.targetB = rgbData[idx + 2];
                    p.morphing = true;
                    p.targetOpacity = 1.0f;
                } else {
                    // New particle: spawn at center, morph outward
                    _initParticle(particleIdx, screenX, screenY,
                                  rgbData[idx], rgbData[idx + 1], rgbData[idx + 2]);

                    if (_hasImage) {
                        // Spawn from center for dramatic effect
                        p.x = SCREEN_WIDTH / 2.0f;
                        p.y = SCREEN_HEIGHT / 2.0f;
                    }
                }

                particleIdx++;
            }
        }

        // Handle particles that are no longer needed
        for (int i = particleIdx; i < activeCount; i++) {
            particles[i].targetOpacity = 0.0f;
            particles[i].morphing = false;
        }

        activeCount = max(particleIdx, activeCount);
        _hasImage = true;
        _clearing = false;

        USBSerial.printf("Particles: %d active from %dx%d image (%d valid pixels)\n",
                         particleIdx, imgW, imgH, validCount);
    }

    /**
     * Update particle config — smooth transition to new values.
     */
    void updateConfig(const ParticleConfig& newConfig) {
        targetConfig = newConfig;
    }

    /**
     * Parse config from JSON and apply.
     */
    void parseConfig(const JsonObject& cfg) {
        if (cfg.containsKey("particle_count"))
            targetConfig.particle_count = constrain(cfg["particle_count"].as<int>(), 100, MAX_PARTICLES);
        if (cfg.containsKey("particle_size"))
            targetConfig.particle_size = constrain(cfg["particle_size"].as<float>(), 0.5f, 8.0f);
        if (cfg.containsKey("particle_speed"))
            targetConfig.particle_speed = constrain(cfg["particle_speed"].as<float>(), 0.1f, 5.0f);
        if (cfg.containsKey("dispersion"))
            targetConfig.dispersion = constrain(cfg["dispersion"].as<float>(), 1.0f, 200.0f);
        if (cfg.containsKey("opacity"))
            targetConfig.opacity = constrain(cfg["opacity"].as<float>(), 0.0f, 1.0f);
        if (cfg.containsKey("pulse_speed"))
            targetConfig.pulse_speed = constrain(cfg["pulse_speed"].as<float>(), 0.1f, 5.0f);
        if (cfg.containsKey("rotation_speed"))
            targetConfig.rotation_speed = cfg["rotation_speed"].as<float>();
        if (cfg.containsKey("link_count"))
            targetConfig.link_count = constrain(cfg["link_count"].as<int>(), 0, 100);
        if (cfg.containsKey("link_opacity"))
            targetConfig.link_opacity = constrain(cfg["link_opacity"].as<float>(), 0.0f, 1.0f);

        if (cfg.containsKey("animation")) {
            String anim = cfg["animation"].as<String>();
            if (anim == "float") targetConfig.animation = ANIM_FLOAT;
            else if (anim == "drift") targetConfig.animation = ANIM_DRIFT;
            else if (anim == "swirl_inward") targetConfig.animation = ANIM_SWIRL_INWARD;
            else if (anim == "pulse_outward") targetConfig.animation = ANIM_PULSE_OUTWARD;
        }

        if (cfg.containsKey("shape")) {
            String shape = cfg["shape"].as<String>();
            if (shape == "circle") targetConfig.shape = SHAPE_CIRCLE;
            else if (shape == "square") targetConfig.shape = SHAPE_SQUARE;
            else if (shape == "star") targetConfig.shape = SHAPE_STAR;
        }

        if (cfg.containsKey("bg_color")) {
            String bg = cfg["bg_color"].as<String>();
            if (bg.startsWith("#") && bg.length() == 7) {
                uint32_t rgb = strtoul(bg.c_str() + 1, NULL, 16);
                uint8_t r = (rgb >> 16) & 0xFF;
                uint8_t g = (rgb >> 8) & 0xFF;
                uint8_t b = rgb & 0xFF;
                targetConfig.bg_color = _rgb565(r, g, b);
            }
        }
    }

    /**
     * Update physics — call every frame with delta time in seconds.
     */
    void update(float dt) {
        if (!particles) return;

        // Lerp config toward target
        _lerpConfig(dt);

        // Update global state
        globalRotation += config.rotation_speed * dt;
        if (globalRotation > 360.0f) globalRotation -= 360.0f;

        pulsePhase += config.pulse_speed * dt;
        if (pulsePhase > TWO_PI) pulsePhase -= TWO_PI;

        float centerX = SCREEN_WIDTH / 2.0f;
        float centerY = SCREEN_HEIGHT / 2.0f;

        // Startup animation
        if (_startupActive) {
            _startupPhase += dt;
            if (_startupPhase > 3.0f) {
                _startupActive = false;
            }
        }

        // Update each particle
        int effectiveCount = min(activeCount, config.particle_count);

        for (int i = 0; i < activeCount; i++) {
            Particle& p = particles[i];

            // Fade out particles beyond current count
            if (i >= effectiveCount) {
                p.opacity = max(0.0f, p.opacity - FADE_OUT_SPEED * dt);
                continue;
            }

            // Handle morphing (new image transition)
            if (p.morphing) {
                float morphSpeed = POSITION_LERP_SPEED * dt;
                p.homeX += (p.targetHomeX - p.homeX) * morphSpeed;
                p.homeY += (p.targetHomeY - p.homeY) * morphSpeed;
                p.homeZ += (p.targetHomeZ - p.homeZ) * morphSpeed;

                // Morph color
                p.r = _lerpByte(p.r, p.targetR, morphSpeed);
                p.g = _lerpByte(p.g, p.targetG, morphSpeed);
                p.b = _lerpByte(p.b, p.targetB, morphSpeed);

                // Check if morph is complete
                float dist = abs(p.homeX - p.targetHomeX) + abs(p.homeY - p.targetHomeY);
                if (dist < 1.0f) {
                    p.homeX = p.targetHomeX;
                    p.homeY = p.targetHomeY;
                    p.homeZ = p.targetHomeZ;
                    p.r = p.targetR;
                    p.g = p.targetG;
                    p.b = p.targetB;
                    p.morphing = false;
                }
            }

            // Fade opacity
            float targetOp = _clearing ? 0.0f : p.targetOpacity * config.opacity;
            if (p.opacity < targetOp) {
                p.opacity = min(targetOp, p.opacity + 2.0f * dt);
            } else if (p.opacity > targetOp) {
                p.opacity = max(targetOp, p.opacity - FADE_OUT_SPEED * dt);
            }

            // Update orbit angles
            p.angleXY += p.angularSpeedXY * config.particle_speed * dt;
            p.angleXZ += p.angularSpeedXZ * config.particle_speed * dt;

            // Target orbit radius based on dispersion
            float targetRadius = config.dispersion * (0.5f + 0.5f * sin(p.phase + pulsePhase));
            p.orbitRadius += (targetRadius - p.orbitRadius) * 2.0f * dt;

            // Calculate position based on animation type
            float animX = 0, animY = 0;

            switch (config.animation) {
                case ANIM_FLOAT: {
                    // Gentle random drift around home
                    animX = cos(p.angleXY) * p.orbitRadius;
                    animY = sin(p.angleXZ) * p.orbitRadius;
                    break;
                }

                case ANIM_DRIFT: {
                    // Very slow lazy movement
                    animX = cos(p.angleXY * 0.3f) * p.orbitRadius * 0.5f;
                    animY = sin(p.angleXZ * 0.3f) * p.orbitRadius * 0.5f;
                    break;
                }

                case ANIM_SWIRL_INWARD: {
                    // Orbit toward center (thinking)
                    float dx = p.homeX - centerX;
                    float dy = p.homeY - centerY;
                    float dist = sqrtf(dx * dx + dy * dy) + 1.0f;
                    float angle = atan2f(dy, dx) + p.angleXY;

                    // Pull inward slightly
                    float pullFactor = 0.7f + 0.3f * sin(pulsePhase + p.phase);
                    animX = cos(angle) * p.orbitRadius * pullFactor - dx * 0.1f * sin(pulsePhase);
                    animY = sin(angle) * p.orbitRadius * pullFactor - dy * 0.1f * sin(pulsePhase);
                    break;
                }

                case ANIM_PULSE_OUTWARD: {
                    // Push outward in waves from center (talking)
                    float dx = p.homeX - centerX;
                    float dy = p.homeY - centerY;
                    float dist = sqrtf(dx * dx + dy * dy) + 1.0f;

                    float pulseWave = sin(pulsePhase - dist * 0.02f);
                    float pushAmount = pulseWave * config.dispersion * 0.3f;

                    animX = cos(p.angleXY) * p.orbitRadius + (dx / dist) * pushAmount;
                    animY = sin(p.angleXZ) * p.orbitRadius + (dy / dist) * pushAmount;
                    break;
                }
            }

            // Apply global rotation
            if (abs(config.rotation_speed) > 0.01f) {
                float rad = globalRotation * DEG_TO_RAD;
                float cosR = cos(rad);
                float sinR = sin(rad);
                float rx = animX * cosR - animY * sinR;
                float ry = animX * sinR + animY * cosR;
                animX = rx;
                animY = ry;
            }

            // Lerp position toward target (smooth movement)
            float targetX = p.homeX + animX;
            float targetY = p.homeY + animY;
            p.x += (targetX - p.x) * min(1.0f, 4.0f * dt);
            p.y += (targetY - p.y) * min(1.0f, 4.0f * dt);
        }

        // Remove fully faded particles from the end
        while (activeCount > 0 && particles[activeCount - 1].opacity < 0.01f
               && !particles[activeCount - 1].morphing) {
            activeCount--;
        }
    }

    /**
     * Render all particles to the display.
     */
    void render(Arduino_GFX* gfx) {
        if (!particles || !gfx) return;

        // Clear background
        gfx->fillScreen(config.bg_color);

        int effectiveCount = min(activeCount, (int)MAX_PARTICLES);
        int size = max(1, (int)(config.particle_size + 0.5f));

        for (int i = 0; i < effectiveCount; i++) {
            Particle& p = particles[i];

            if (p.opacity < 0.05f) continue;

            // Screen bounds check
            int sx = (int)(p.x + 0.5f);
            int sy = (int)(p.y + 0.5f);

            if (sx < -size || sx >= SCREEN_WIDTH + size ||
                sy < -size || sy >= SCREEN_HEIGHT + size) {
                continue;
            }

            // Apply opacity to color
            uint8_t drawR = (uint8_t)(p.r * p.opacity);
            uint8_t drawG = (uint8_t)(p.g * p.opacity);
            uint8_t drawB = (uint8_t)(p.b * p.opacity);

            uint16_t color = _rgb565(drawR, drawG, drawB);

            // Draw based on shape
            if (config.shape == SHAPE_CIRCLE) {
                if (size <= 1) {
                    gfx->drawPixel(sx, sy, color);
                } else {
                    gfx->fillCircle(sx, sy, size, color);
                }
            } else if (config.shape == SHAPE_SQUARE) {
                gfx->fillRect(sx - size, sy - size, size * 2, size * 2, color);
            }
            // Star shape skipped for perf — circle is fastest
        }

        // Draw links between nearby particles (if enabled)
        if (config.link_count > 0 && config.link_opacity > 0.01f) {
            _renderLinks(gfx, effectiveCount);
        }
    }

    /**
     * Start clearing — fade all particles out.
     */
    void clear() {
        _clearing = true;
    }

    /**
     * Start the startup animation (particles emerge from center).
     */
    void startStartup() {
        _startupActive = true;
        _startupPhase = 0.0f;

        // Create some initial particles at center
        int count = min(DEFAULT_PARTICLE_COUNT, MAX_PARTICLES);
        float centerX = SCREEN_WIDTH / 2.0f;
        float centerY = SCREEN_HEIGHT / 2.0f;

        for (int i = 0; i < count; i++) {
            _initParticle(i,
                centerX + random(-SCREEN_WIDTH / 3, SCREEN_WIDTH / 3),
                centerY + random(-SCREEN_HEIGHT / 3, SCREEN_HEIGHT / 3),
                0, 200 + random(55), 200 + random(55));  // Cyan-ish

            // Start all at center
            particles[i].x = centerX;
            particles[i].y = centerY;
            particles[i].opacity = 0.0f;
            particles[i].targetOpacity = 0.8f;
        }

        activeCount = count;
        _hasImage = false;
    }

    // Accessors
    int getActiveCount() const { return activeCount; }
    bool hasImage() const { return _hasImage; }
    bool isClearing() const { return _clearing; }
    const ParticleConfig& getConfig() const { return config; }

private:
    Particle* particles;
    int activeCount;
    ParticleConfig config;
    ParticleConfig targetConfig;
    float globalRotation;
    float pulsePhase;
    bool _clearing;
    float _startupPhase;
    bool _startupActive;
    bool _hasImage;

    /**
     * Initialize a single particle.
     */
    void _initParticle(int idx, float homeX, float homeY, uint8_t r, uint8_t g, uint8_t b) {
        if (idx >= MAX_PARTICLES) return;

        Particle& p = particles[idx];
        p.homeX = homeX;
        p.homeY = homeY;
        p.homeZ = 0.0f;
        p.targetHomeX = homeX;
        p.targetHomeY = homeY;
        p.targetHomeZ = 0.0f;
        p.x = homeX;
        p.y = homeY;
        p.z = 0.0f;
        p.r = r;
        p.g = g;
        p.b = b;
        p.targetR = r;
        p.targetG = g;
        p.targetB = b;
        p.opacity = 0.0f;
        p.targetOpacity = 1.0f;
        p.morphing = false;

        // Random orbit parameters for variation
        p.angleXY = random(0, 628) / 100.0f;  // 0 to 2*PI
        p.angleXZ = random(0, 628) / 100.0f;
        p.angularSpeedXY = 0.5f + random(0, 100) / 100.0f;
        p.angularSpeedXZ = 0.3f + random(0, 100) / 150.0f;
        p.orbitRadius = random(0, 100) / 100.0f * config.dispersion;
        p.phase = random(0, 628) / 100.0f;
    }

    /**
     * Smoothly interpolate config values toward target.
     */
    void _lerpConfig(float dt) {
        float t = min(1.0f, CONFIG_LERP_SPEED * dt);

        config.particle_size += (targetConfig.particle_size - config.particle_size) * t;
        config.particle_speed += (targetConfig.particle_speed - config.particle_speed) * t;
        config.dispersion += (targetConfig.dispersion - config.dispersion) * t;
        config.opacity += (targetConfig.opacity - config.opacity) * t;
        config.pulse_speed += (targetConfig.pulse_speed - config.pulse_speed) * t;
        config.rotation_speed += (targetConfig.rotation_speed - config.rotation_speed) * t;
        config.link_opacity += (targetConfig.link_opacity - config.link_opacity) * t;

        // Discrete values — snap immediately
        config.animation = targetConfig.animation;
        config.shape = targetConfig.shape;
        config.bg_color = targetConfig.bg_color;
        config.particle_count = targetConfig.particle_count;
        config.link_count = targetConfig.link_count;
        config.color_mode = targetConfig.color_mode;
    }

    /**
     * Lerp a byte value.
     */
    static uint8_t _lerpByte(uint8_t a, uint8_t b, float t) {
        return (uint8_t)(a + (int)(((int)b - (int)a) * t));
    }

    /**
     * Convert 8-bit RGB to RGB565.
     */
    static uint16_t _rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    /**
     * Render connecting lines between nearby particles.
     * Limited to link_count lines for performance.
     */
    void _renderLinks(Arduino_GFX* gfx, int count) {
        int linksDrawn = 0;
        int maxLinks = config.link_count;
        float maxDist = config.dispersion * 2.0f;
        float maxDistSq = maxDist * maxDist;

        uint8_t linkAlpha = (uint8_t)(config.link_opacity * 255);
        uint16_t linkColor = _rgb565(linkAlpha / 4, linkAlpha / 2, linkAlpha / 2);

        // Sample random pairs (don't check every combo — O(n²) is too slow)
        for (int attempt = 0; attempt < maxLinks * 3 && linksDrawn < maxLinks; attempt++) {
            int a = random(0, count);
            int b = random(0, count);
            if (a == b) continue;

            float dx = particles[a].x - particles[b].x;
            float dy = particles[a].y - particles[b].y;
            float distSq = dx * dx + dy * dy;

            if (distSq < maxDistSq && distSq > 4.0f) {
                gfx->drawLine(
                    (int)particles[a].x, (int)particles[a].y,
                    (int)particles[b].x, (int)particles[b].y,
                    linkColor
                );
                linksDrawn++;
            }
        }
    }
};

#endif // PARTICLE_SYSTEM_H
