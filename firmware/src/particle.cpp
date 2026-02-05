/**
 * Ada Particles - Particle Pool Implementation
 */

#include "particle.h"

// Global instance
ParticlePool particlePool;

// ============================================
// Constructor
// ============================================

ParticlePool::ParticlePool() 
    : _particles(nullptr), _activeCount(0) {
}

// ============================================
// Initialization
// ============================================

bool ParticlePool::init() {
    size_t bytes = sizeof(Particle) * MAX_PARTICLES;
    
    // Allocate in PSRAM
    _particles = (Particle*)ps_malloc(bytes);
    if (!_particles) {
        _particles = (Particle*)malloc(bytes);
    }
    
    if (!_particles) {
        Serial.println("ERROR: Failed to allocate particle pool!");
        return false;
    }
    
    // Initialize all particles as inactive
    memset(_particles, 0, bytes);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        _particles[i].state = PARTICLE_INACTIVE;
    }
    
    _activeCount = 0;
    
    Serial.printf("Particle pool: %d particles (%d bytes)\n", 
                  MAX_PARTICLES, bytes);
    
    return true;
}

// ============================================
// Access
// ============================================

Particle& ParticlePool::get(int index) {
    static Particle dummy = {0};
    if (!_particles || index < 0 || index >= MAX_PARTICLES) {
        return dummy;
    }
    return _particles[index];
}

const Particle& ParticlePool::get(int index) const {
    static Particle dummy = {0};
    if (!_particles || index < 0 || index >= MAX_PARTICLES) {
        return dummy;
    }
    return _particles[index];
}

// ============================================
// Activation / Deactivation
// ============================================

int ParticlePool::findInactiveSlot() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (_particles[i].state == PARTICLE_INACTIVE) {
            return i;
        }
    }
    return -1;
}

void ParticlePool::initParticle(Particle& p, fixed_t x, fixed_t y) {
    p.x = x;
    p.y = y;
    p.vx = 0;
    p.vy = 0;
    p.targetX = 0;
    p.targetY = 0;
    p.hasTarget = false;
    
    // Random size distribution (favor smaller particles)
    uint8_t sizeRoll = random(100);
    if (sizeRoll < 60) {
        p.sizeIdx = 0;  // 60% small
    } else if (sizeRoll < 90) {
        p.sizeIdx = 1;  // 30% medium
    } else {
        p.sizeIdx = 2;  // 10% large
    }
    
    // Random brightness variation for visual interest
    p.brightness = 180 + random(76);  // 180-255
    
    // Random animation phase
    p.phase = INT_TO_FIXED(random(1000)) / 1000;
    
    // Random noise offset (so particles don't all move in sync)
    p.noiseOffsetX = INT_TO_FIXED(random(10000));
    p.noiseOffsetY = INT_TO_FIXED(random(10000));
    
    // Start fading in
    p.state = PARTICLE_FADING_IN;
    p.fadeProgress = 0;
}

int ParticlePool::activate() {
    // Random position within screen bounds (with margin)
    int margin = 50;
    fixed_t x = INT_TO_FIXED(margin + random(SCREEN_WIDTH - margin * 2));
    fixed_t y = INT_TO_FIXED(margin + random(SCREEN_HEIGHT - margin * 2));
    
    return activateAt(x, y);
}

int ParticlePool::activateAt(fixed_t x, fixed_t y) {
    if (!_particles) return -1;
    
    int slot = findInactiveSlot();
    if (slot < 0) return -1;
    
    Particle& p = _particles[slot];
    initParticle(p, x, y);
    
    _activeCount++;
    return slot;
}

void ParticlePool::deactivate(int index) {
    if (!_particles || index < 0 || index >= MAX_PARTICLES) return;
    
    if (_particles[index].state != PARTICLE_INACTIVE) {
        _particles[index].state = PARTICLE_INACTIVE;
        _activeCount--;
        if (_activeCount < 0) _activeCount = 0;
    }
}

void ParticlePool::startFadeOut(int index) {
    if (!_particles || index < 0 || index >= MAX_PARTICLES) return;
    
    if (_particles[index].state == PARTICLE_ACTIVE || 
        _particles[index].state == PARTICLE_FADING_IN) {
        _particles[index].state = PARTICLE_FADING_OUT;
        _particles[index].fadeProgress = 255;
    }
}

// ============================================
// Fade Management
// ============================================

void ParticlePool::updateFades(float dt) {
    if (!_particles) return;
    
    // Fade speed: 0 to 255 in ~0.5 seconds
    int fadeStep = (int)(dt * 512.0f);
    if (fadeStep < 1) fadeStep = 1;
    
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = _particles[i];
        
        if (p.state == PARTICLE_FADING_IN) {
            p.fadeProgress += fadeStep;
            if (p.fadeProgress >= 255) {
                p.fadeProgress = 255;
                p.state = PARTICLE_ACTIVE;
            }
        } else if (p.state == PARTICLE_FADING_OUT) {
            p.fadeProgress -= fadeStep;
            if (p.fadeProgress <= 0) {
                p.fadeProgress = 0;
                p.state = PARTICLE_INACTIVE;
                _activeCount--;
                if (_activeCount < 0) _activeCount = 0;
            }
        }
    }
}

// ============================================
// Clear
// ============================================

void ParticlePool::clear() {
    if (!_particles) return;
    
    for (int i = 0; i < MAX_PARTICLES; i++) {
        _particles[i].state = PARTICLE_INACTIVE;
    }
    _activeCount = 0;
}
