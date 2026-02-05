/**
 * Ada Particles - Particle Data Structure
 * 
 * Defines the Particle struct and related types.
 * Particles use fixed-point math for efficient physics.
 */

#ifndef PARTICLE_H
#define PARTICLE_H

#include <Arduino.h>
#include "fixed_math.h"
#include "../config.h"

// ============================================
// Particle Structure
// ============================================

struct Particle {
    // Position (16.16 fixed-point, screen coordinates)
    fixed_t x;
    fixed_t y;
    
    // Velocity (16.16 fixed-point)
    fixed_t vx;
    fixed_t vy;
    
    // Target position for formations (-1 = no target, free floating)
    fixed_t targetX;
    fixed_t targetY;
    bool hasTarget;
    
    // Visual properties
    uint8_t sizeIdx;       // 0=small, 1=medium, 2=large
    uint8_t brightness;    // 0-255 (varies per particle for visual interest)
    
    // Animation state
    fixed_t phase;         // Random phase offset for variation
    fixed_t noiseOffsetX;  // Offset into noise field
    fixed_t noiseOffsetY;
    
    // Lifecycle
    uint8_t state;         // 0=inactive, 1=active, 2=fading in, 3=fading out
    uint8_t fadeProgress;  // 0-255 for fade in/out
};

// Particle states
enum ParticleState : uint8_t {
    PARTICLE_INACTIVE = 0,
    PARTICLE_ACTIVE = 1,
    PARTICLE_FADING_IN = 2,
    PARTICLE_FADING_OUT = 3
};

// ============================================
// Particle Pool
// ============================================

class ParticlePool {
public:
    ParticlePool();
    
    /**
     * Initialize particle pool in PSRAM.
     * @return true if successful
     */
    bool init();
    
    /**
     * Get particle by index.
     */
    Particle& get(int index);
    const Particle& get(int index) const;
    
    /**
     * Activate a particle at random position.
     * @return Index of activated particle, or -1 if pool full
     */
    int activate();
    
    /**
     * Activate a particle at specific position.
     */
    int activateAt(fixed_t x, fixed_t y);
    
    /**
     * Deactivate a particle.
     */
    void deactivate(int index);
    
    /**
     * Start fading out a particle.
     */
    void startFadeOut(int index);
    
    /**
     * Get number of active particles.
     */
    int getActiveCount() const { return _activeCount; }
    
    /**
     * Get maximum capacity.
     */
    int getCapacity() const { return MAX_PARTICLES; }
    
    /**
     * Check if pool is valid.
     */
    bool isValid() const { return _particles != nullptr; }
    
    /**
     * Update fade progress for all fading particles.
     * @param dt Delta time in seconds
     */
    void updateFades(float dt);
    
    /**
     * Clear all particles (deactivate).
     */
    void clear();

private:
    Particle* _particles;
    int _activeCount;
    
    // Find first inactive slot
    int findInactiveSlot();
    
    // Initialize a particle with random properties
    void initParticle(Particle& p, fixed_t x, fixed_t y);
};

// Global instance
extern ParticlePool particlePool;

#endif // PARTICLE_H
