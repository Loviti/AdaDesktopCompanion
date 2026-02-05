/**
 * Ada Particles - Particle System Implementation
 */

#include "particle_system.h"
#include <math.h>

// Global instance
ParticleSystem particleSystem;

// ============================================
// Constructor
// ============================================

ParticleSystem::ParticleSystem()
    : _gfx(nullptr)
    , _state(STATE_STARTING)
    , _ready(false)
    , _disconnected(false)
    , _currentFormation(FORMATION_IDLE)
    , _targetFormation(FORMATION_IDLE)
    , _transitionProgress(1.0f)
    , _transitionSpeed(0.5f)
    , _valence(0.0f)
    , _arousal(0.3f)
    , _targetValence(0.0f)
    , _targetArousal(0.3f)
    , _currentColor(0x07FF)  // Cyan
    , _noiseTime(0)
    , _targetParticleCount(DEFAULT_PARTICLE_COUNT)
    , _lastFrameTime(0)
    , _fps(0)
    , _frameCount(0)
    , _fpsUpdateTime(0) {
}

// ============================================
// Initialization
// ============================================

bool ParticleSystem::init(Arduino_GFX* gfx) {
    _gfx = gfx;
    
    Serial.println("Initializing particle system...");
    
    // Initialize noise
    noise_init(esp_random());
    Serial.println("  Noise initialized");
    
    // Generate particle sprites
    if (!particleSprites.generate()) {
        Serial.println("ERROR: Failed to generate sprites");
        return false;
    }
    
    // Initialize framebuffer
    if (!_framebuffer.init(gfx)) {
        Serial.println("ERROR: Failed to init framebuffer");
        return false;
    }
    
    // Connect sprites to framebuffer
    _framebuffer.setParticleSprites(
        particleSprites.getSpriteArray(),
        particleSprites.getSizesArray()
    );
    
    // Initialize particle pool
    if (!particlePool.init()) {
        Serial.println("ERROR: Failed to init particle pool");
        return false;
    }
    
    // Spawn initial particles at center
    fixed_t centerX = INT_TO_FIXED(SCREEN_CENTER_X);
    fixed_t centerY = INT_TO_FIXED(SCREEN_CENTER_Y);
    
    for (int i = 0; i < _targetParticleCount; i++) {
        // Start all particles at center, they'll drift outward
        fixed_t x = centerX + INT_TO_FIXED(random(-20, 20));
        fixed_t y = centerY + INT_TO_FIXED(random(-20, 20));
        particlePool.activateAt(x, y);
    }
    
    _lastFrameTime = millis();
    _fpsUpdateTime = millis();
    _state = STATE_IDLE;
    _ready = true;
    
    Serial.println("Particle system ready!");
    Serial.printf("  PSRAM free: %d bytes\n", ESP.getFreePsram());
    
    return true;
}

// ============================================
// Update
// ============================================

void ParticleSystem::update(float dt) {
    if (!_ready) return;
    
    // Update noise time
    _noiseTime += FLOAT_TO_FIXED(dt * NOISE_TIME_SPEED);
    
    // Update mood colors
    updateColor(dt);
    
    // Update formation transition
    if (_transitionProgress < 1.0f) {
        _transitionProgress += _transitionSpeed * dt;
        if (_transitionProgress >= 1.0f) {
            _transitionProgress = 1.0f;
            _currentFormation = _targetFormation;
        }
        updateFormationTargets();
    }
    
    // Update particle count
    adjustParticleCount(dt);
    
    // Update particle fades
    particlePool.updateFades(dt);
    
    // Update physics for each particle
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = particlePool.get(i);
        if (p.state == PARTICLE_INACTIVE) continue;
        
        updateParticlePhysics(p, dt);
    }
    
    // Update FPS counter
    _frameCount++;
    unsigned long now = millis();
    if (now - _fpsUpdateTime >= 1000) {
        _fps = _frameCount * 1000.0f / (now - _fpsUpdateTime);
        _frameCount = 0;
        _fpsUpdateTime = now;
    }
}

void ParticleSystem::updateParticlePhysics(Particle& p, float dt) {
    // Apply noise-based wandering
    applyNoise(p, dt);
    
    // Apply formation attraction if particle has target
    if (p.hasTarget) {
        applyFormationAttraction(p, dt);
    }
    
    // Apply soft center attraction
    applyCenterAttraction(p, dt);
    
    // Apply damping
    applyDamping(p);
    
    // Clamp velocity
    clampVelocity(p);
    
    // Integrate position
    integratePosition(p, dt);
}

void ParticleSystem::applyNoise(Particle& p, float dt) {
    // Sample noise at particle's offset position
    uint32_t nx = (uint32_t)(p.x + p.noiseOffsetX);
    uint32_t ny = (uint32_t)(p.y + p.noiseOffsetY);
    uint32_t nt = (uint32_t)_noiseTime;
    
    // Scale coordinates for appropriate noise frequency
    uint32_t scaledX = (uint64_t)nx * (uint32_t)(NOISE_SCALE * 65536) >> 16;
    uint32_t scaledY = (uint64_t)ny * (uint32_t)(NOISE_SCALE * 65536) >> 16;
    
    // Get curl noise for divergence-free flow
    fixed_t noiseVX, noiseVY;
    curl_noise_2d(scaledX, scaledY, nt, &noiseVX, &noiseVY);
    
    // Scale and apply to velocity
    fixed_t strength = FLOAT_TO_FIXED(WANDER_STRENGTH * dt);
    p.vx += fixed_mul(noiseVX, strength);
    p.vy += fixed_mul(noiseVY, strength);
}

void ParticleSystem::applyFormationAttraction(Particle& p, float dt) {
    if (!p.hasTarget) return;
    
    // Vector to target
    fixed_t dx = p.targetX - p.x;
    fixed_t dy = p.targetY - p.y;
    
    // Spring force
    fixed_t spring = FLOAT_TO_FIXED(SPRING_K * dt);
    
    // Blend based on transition progress and formation tightness
    float blend = _transitionProgress * FORMATION_TIGHTNESS;
    spring = fixed_mul(spring, FLOAT_TO_FIXED(blend));
    
    p.vx += fixed_mul(dx, spring);
    p.vy += fixed_mul(dy, spring);
}

void ParticleSystem::applyCenterAttraction(Particle& p, float dt) {
    // Soft pull toward screen center to prevent drift
    fixed_t centerX = INT_TO_FIXED(SCREEN_CENTER_X);
    fixed_t centerY = INT_TO_FIXED(SCREEN_CENTER_Y);
    
    fixed_t dx = centerX - p.x;
    fixed_t dy = centerY - p.y;
    
    // Weaker if has formation target
    float pullStrength = p.hasTarget ? CENTER_PULL * 0.3f : CENTER_PULL;
    fixed_t pull = FLOAT_TO_FIXED(pullStrength * dt);
    
    p.vx += fixed_mul(dx, pull);
    p.vy += fixed_mul(dy, pull);
}

void ParticleSystem::applyDamping(Particle& p) {
    // Velocity damping for smooth motion
    fixed_t damping = FLOAT_TO_FIXED(DAMPING);
    p.vx = fixed_mul(p.vx, damping);
    p.vy = fixed_mul(p.vy, damping);
}

void ParticleSystem::clampVelocity(Particle& p) {
    // Prevent particles from moving too fast
    fixed_t maxV = FLOAT_TO_FIXED(MAX_VELOCITY);
    
    if (p.vx > maxV) p.vx = maxV;
    if (p.vx < -maxV) p.vx = -maxV;
    if (p.vy > maxV) p.vy = maxV;
    if (p.vy < -maxV) p.vy = -maxV;
}

void ParticleSystem::integratePosition(Particle& p, float dt) {
    p.x += p.vx;
    p.y += p.vy;
    
    // Soft boundary wrapping (particles wrap around screen edges)
    int margin = 30;
    fixed_t minX = INT_TO_FIXED(-margin);
    fixed_t maxX = INT_TO_FIXED(SCREEN_WIDTH + margin);
    fixed_t minY = INT_TO_FIXED(-margin);
    fixed_t maxY = INT_TO_FIXED(SCREEN_HEIGHT + margin);
    
    if (p.x < minX) p.x = maxX - FIXED_ONE;
    if (p.x > maxX) p.x = minX + FIXED_ONE;
    if (p.y < minY) p.y = maxY - FIXED_ONE;
    if (p.y > maxY) p.y = minY + FIXED_ONE;
}

// ============================================
// Formation Management
// ============================================

void ParticleSystem::setFormation(FormationType formation, uint16_t transitionMs) {
    if (formation >= FORMATION_COUNT) formation = FORMATION_IDLE;
    
    _targetFormation = formation;
    _transitionProgress = 0.0f;
    _transitionSpeed = 1000.0f / (float)transitionMs;
    _state = STATE_TRANSITIONING;
    
    updateFormationTargets();
}

void ParticleSystem::clearFormation(uint16_t transitionMs) {
    setFormation(FORMATION_IDLE, transitionMs);
}

void ParticleSystem::updateFormationTargets() {
    int activeCount = particlePool.getActiveCount();
    if (activeCount == 0) return;
    
    if (_targetFormation == FORMATION_IDLE) {
        // Clear all targets for idle
        clearAllTargets();
        return;
    }
    
    // Assign formation targets to particles
    int targetIdx = 0;
    for (int i = 0; i < MAX_PARTICLES && targetIdx < activeCount; i++) {
        Particle& p = particlePool.get(i);
        if (p.state == PARTICLE_INACTIVE) continue;
        
        fixed_t tx, ty;
        getFormationPoint(_targetFormation, targetIdx, activeCount, tx, ty);
        
        p.targetX = tx;
        p.targetY = ty;
        p.hasTarget = true;
        
        targetIdx++;
    }
}

void ParticleSystem::clearAllTargets() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = particlePool.get(i);
        p.hasTarget = false;
    }
}

void ParticleSystem::getFormationPoint(FormationType formation, int index, int total,
                                       fixed_t& outX, fixed_t& outY) {
    float t = (float)index / (float)max(1, total - 1);
    float cx = SCREEN_CENTER_X;
    float cy = SCREEN_CENTER_Y;
    float radius = min(SCREEN_WIDTH, SCREEN_HEIGHT) * 0.35f;
    
    float x = cx, y = cy;
    
    switch (formation) {
        case FORMATION_CLOUD: {
            // Fluffy cloud shape using multiple overlapping circles
            float angle = t * 2.0f * PI;
            float r = radius * (0.4f + 0.6f * sin(angle * 3.0f + index * 0.1f));
            r *= 0.5f + 0.5f * cos(angle * 2.0f);
            x = cx + cos(angle) * r * 1.3f;
            y = cy + sin(angle) * r * 0.6f - radius * 0.1f;
            // Add some randomness for fluffy look
            x += sin(index * 1.3f) * 20.0f;
            y += cos(index * 1.7f) * 15.0f;
            break;
        }
        
        case FORMATION_SUN: {
            if (index < total * 0.3f) {
                // Center cluster
                float angle = t * 10.0f * PI;
                float r = (t * 0.3f / 0.3f) * radius * 0.4f;
                x = cx + cos(angle) * r;
                y = cy + sin(angle) * r;
            } else {
                // Rays
                float rayT = (t - 0.3f) / 0.7f;
                int numRays = 8;
                int rayIndex = (int)(rayT * numRays) % numRays;
                float rayAngle = rayIndex * (2.0f * PI / numRays);
                float rayProgress = fmod(rayT * numRays, 1.0f);
                float r = radius * (0.5f + rayProgress * 0.5f);
                x = cx + cos(rayAngle) * r;
                y = cy + sin(rayAngle) * r;
            }
            break;
        }
        
        case FORMATION_RAIN: {
            // Vertical columns
            int numColumns = 12;
            int col = index % numColumns;
            float colX = (col + 0.5f) / numColumns * SCREEN_WIDTH;
            float rowT = (float)(index / numColumns) / (float)(total / numColumns);
            x = colX + sin(index * 0.5f) * 10.0f;
            y = rowT * SCREEN_HEIGHT;
            break;
        }
        
        case FORMATION_SNOW: {
            // Scattered random-ish pattern
            float angle = index * 2.399f;  // Golden angle
            float r = sqrt(t) * radius * 1.2f;
            x = cx + cos(angle) * r;
            y = cy + sin(angle) * r;
            break;
        }
        
        case FORMATION_HEART: {
            // Heart curve
            float ht = t * 2.0f * PI;
            float hx = 16.0f * pow(sin(ht), 3);
            float hy = 13.0f * cos(ht) - 5.0f * cos(2*ht) - 2.0f * cos(3*ht) - cos(4*ht);
            x = cx + hx * (radius / 18.0f);
            y = cy - hy * (radius / 18.0f);  // Flip Y for screen coords
            break;
        }
        
        case FORMATION_THINKING: {
            // Swirling vortex
            float spiralAngle = t * 8.0f * PI;
            float r = t * radius * 0.9f;
            x = cx + cos(spiralAngle) * r;
            y = cy + sin(spiralAngle) * r;
            break;
        }
        
        case FORMATION_WAVE: {
            // Sine wave
            x = t * SCREEN_WIDTH;
            y = cy + sin(t * 4.0f * PI) * radius * 0.5f;
            break;
        }
        
        case FORMATION_DISCONNECTED: {
            // Sad droopy shape
            float angle = t * 2.0f * PI;
            float r = radius * 0.6f;
            x = cx + cos(angle) * r;
            y = cy + sin(angle) * r + abs(cos(angle)) * radius * 0.3f;  // Droopy
            break;
        }
        
        default:
            x = cx;
            y = cy;
    }
    
    outX = FLOAT_TO_FIXED(x);
    outY = FLOAT_TO_FIXED(y);
}

// ============================================
// Color System
// ============================================

void ParticleSystem::setMood(float valence, float arousal) {
    _targetValence = constrain(valence, -1.0f, 1.0f);
    _targetArousal = constrain(arousal, 0.0f, 1.0f);
}

void ParticleSystem::updateColor(float dt) {
    // Smooth lerp toward target mood
    float lerpSpeed = 2.0f * dt;
    _valence += (_targetValence - _valence) * lerpSpeed;
    _arousal += (_targetArousal - _arousal) * lerpSpeed;
    
    _currentColor = calculateMoodColor(_valence, _arousal);
}

uint16_t ParticleSystem::calculateMoodColor(float valence, float arousal) {
    // Map valence to hue: -1 (blue) -> 0 (cyan) -> 1 (gold/yellow)
    // Map arousal to brightness/saturation
    
    uint8_t r, g, b;
    
    if (_disconnected) {
        // Dim blue-gray for disconnected
        r = 30;
        g = 40;
        b = 60;
    } else if (valence < 0) {
        // Negative valence: blue to cyan
        float t = (valence + 1.0f);  // 0 to 1
        r = (uint8_t)(0 + t * 0);
        g = (uint8_t)(100 + t * 155);
        b = (uint8_t)(255);
    } else {
        // Positive valence: cyan to gold
        float t = valence;  // 0 to 1
        r = (uint8_t)(0 + t * 255);
        g = (uint8_t)(255 - t * 35);
        b = (uint8_t)(204 - t * 204);
    }
    
    // Apply arousal as brightness multiplier
    float brightness = 0.5f + 0.5f * arousal;
    if (_disconnected) brightness *= 0.5f;
    
    r = (uint8_t)(r * brightness);
    g = (uint8_t)(g * brightness);
    b = (uint8_t)(b * brightness);
    
    // Convert to RGB565
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void ParticleSystem::setDisconnected(bool disconnected) {
    if (_disconnected != disconnected) {
        _disconnected = disconnected;
        if (disconnected) {
            _state = STATE_DISCONNECTED;
            setFormation(FORMATION_DISCONNECTED, 1000);
        } else {
            _state = STATE_IDLE;
            clearFormation(500);
        }
    }
}

// ============================================
// Particle Count Management
// ============================================

void ParticleSystem::setParticleCount(int count) {
    _targetParticleCount = constrain(count, 50, MAX_PARTICLES);
}

void ParticleSystem::adjustParticleCount(float dt) {
    int current = particlePool.getActiveCount();
    int target = _targetParticleCount;
    
    if (current < target) {
        // Spawn new particles
        int toSpawn = min(5, target - current);  // Max 5 per frame
        for (int i = 0; i < toSpawn; i++) {
            particlePool.activate();
        }
    } else if (current > target) {
        // Fade out excess particles
        int toRemove = min(5, current - target);
        int removed = 0;
        for (int i = MAX_PARTICLES - 1; i >= 0 && removed < toRemove; i--) {
            Particle& p = particlePool.get(i);
            if (p.state == PARTICLE_ACTIVE) {
                particlePool.startFadeOut(i);
                removed++;
            }
        }
    }
}

// ============================================
// Touch Interaction
// ============================================

void ParticleSystem::onTouch(int16_t x, int16_t y) {
    // Particles scatter from touch point
    fixed_t touchX = INT_TO_FIXED(x);
    fixed_t touchY = INT_TO_FIXED(y);
    
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = particlePool.get(i);
        if (p.state == PARTICLE_INACTIVE) continue;
        
        fixed_t dx = p.x - touchX;
        fixed_t dy = p.y - touchY;
        fixed_t distSq = fixed_mul(dx, dx) + fixed_mul(dy, dy);
        
        // Affect particles within radius
        fixed_t radiusSq = INT_TO_FIXED(100 * 100);  // 100 pixel radius
        if (distSq < radiusSq && distSq > FIXED_ONE) {
            fixed_t dist = fixed_sqrt(distSq);
            
            // Normalize direction
            fixed_t nx = fixed_div(dx, dist);
            fixed_t ny = fixed_div(dy, dist);
            
            // Push force inversely proportional to distance
            fixed_t force = fixed_div(INT_TO_FIXED(5), dist / FIXED_ONE + FIXED_ONE);
            
            p.vx += fixed_mul(nx, force);
            p.vy += fixed_mul(ny, force);
        }
    }
}

// ============================================
// Rendering
// ============================================

void ParticleSystem::render() {
    if (!_ready) return;
    
    // Fade existing content (creates trails)
    _framebuffer.fadeFast((uint8_t)(FADE_FACTOR * 256));
    
    // Render all active particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = particlePool.get(i);
        if (p.state == PARTICLE_INACTIVE) continue;
        
        renderParticle(p);
    }
    
    // Push to display
    _framebuffer.pushToDisplay();
}

void ParticleSystem::renderParticle(const Particle& p) {
    int16_t x = FIXED_TO_INT(p.x);
    int16_t y = FIXED_TO_INT(p.y);
    
    // Calculate effective brightness
    uint8_t brightness = p.brightness;
    
    // Apply fade progress
    if (p.state == PARTICLE_FADING_IN || p.state == PARTICLE_FADING_OUT) {
        brightness = (brightness * p.fadeProgress) >> 8;
    }
    
    // Draw soft particle
    _framebuffer.drawSoftParticle(x, y, p.sizeIdx, _currentColor, brightness);
}

int ParticleSystem::getActiveParticles() const {
    return particlePool.getActiveCount();
}
