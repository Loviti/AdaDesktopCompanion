/**
 * Ada Particles - Main Particle System Engine
 * 
 * Orchestrates particle physics, formations, rendering, and state.
 * This is the heart of Ada's visual presence.
 */

#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <Arduino.h>
#include "fixed_math.h"
#include "noise.h"
#include "particle.h"
#include "framebuffer.h"
#include "sprites.h"
#include "../config.h"

// ============================================
// Formation Types
// ============================================

enum FormationType : uint8_t {
    FORMATION_IDLE = 0,      // No formation, free wandering
    FORMATION_CLOUD,         // Fluffy cloud shape
    FORMATION_SUN,           // Sun with radiating rays
    FORMATION_RAIN,          // Vertical rain columns
    FORMATION_SNOW,          // Scattered snowflakes
    FORMATION_HEART,         // Heart shape
    FORMATION_THINKING,      // Swirling vortex
    FORMATION_WAVE,          // Sine wave pattern
    FORMATION_DISCONNECTED,  // Sad/dim disconnected state
    FORMATION_COUNT
};

// ============================================
// System State
// ============================================

enum SystemState : uint8_t {
    STATE_STARTING = 0,      // Initial startup
    STATE_IDLE,              // Normal operation, connected
    STATE_TRANSITIONING,     // Changing formations
    STATE_DISCONNECTED       // No server connection
};

// ============================================
// Particle System Class
// ============================================

class ParticleSystem {
public:
    ParticleSystem();
    
    /**
     * Initialize all subsystems.
     * @param gfx Display to render to
     * @return true if successful
     */
    bool init(Arduino_GFX* gfx);
    
    /**
     * Update physics and state.
     * @param dt Delta time in seconds
     */
    void update(float dt);
    
    /**
     * Render to framebuffer and push to display.
     */
    void render();
    
    /**
     * Set target formation.
     * @param formation Formation type
     * @param transitionMs Transition time in milliseconds
     */
    void setFormation(FormationType formation, uint16_t transitionMs = DEFAULT_TRANSITION_MS);
    
    /**
     * Clear formation (return to idle).
     */
    void clearFormation(uint16_t transitionMs = DEFAULT_TRANSITION_MS);
    
    /**
     * Set mood colors.
     * @param valence -1.0 (concerned) to 1.0 (happy)
     * @param arousal 0.0 (calm) to 1.0 (alert/energetic)
     */
    void setMood(float valence, float arousal);
    
    /**
     * Set disconnected state.
     */
    void setDisconnected(bool disconnected);
    
    /**
     * Set target particle count.
     */
    void setParticleCount(int count);
    
    /**
     * Handle touch at screen coordinates.
     */
    void onTouch(int16_t x, int16_t y);
    
    // Accessors
    FormationType getCurrentFormation() const { return _currentFormation; }
    SystemState getState() const { return _state; }
    int getActiveParticles() const;
    float getFPS() const { return _fps; }
    bool isReady() const { return _ready; }

private:
    // Subsystems
    Framebuffer _framebuffer;
    Arduino_GFX* _gfx;
    
    // State
    SystemState _state;
    bool _ready;
    bool _disconnected;
    
    // Formation
    FormationType _currentFormation;
    FormationType _targetFormation;
    float _transitionProgress;  // 0.0 to 1.0
    float _transitionSpeed;     // Progress per second
    
    // Mood (drives color)
    float _valence;       // -1 to 1
    float _arousal;       // 0 to 1
    float _targetValence;
    float _targetArousal;
    
    // Current display color (RGB565)
    uint16_t _currentColor;
    
    // Physics time
    fixed_t _noiseTime;
    
    // Target particle count
    int _targetParticleCount;
    
    // Performance tracking
    unsigned long _lastFrameTime;
    float _fps;
    int _frameCount;
    unsigned long _fpsUpdateTime;
    
    // ============================================
    // Internal Methods
    // ============================================
    
    // Physics update
    void updateParticlePhysics(Particle& p, float dt);
    void applyNoise(Particle& p, float dt);
    void applyFormationAttraction(Particle& p, float dt);
    void applyCenterAttraction(Particle& p, float dt);
    void applyDamping(Particle& p);
    void clampVelocity(Particle& p);
    void integratePosition(Particle& p, float dt);
    
    // Formation management
    void updateFormationTargets();
    void clearAllTargets();
    void getFormationPoint(FormationType formation, int index, int total,
                          fixed_t& outX, fixed_t& outY);
    
    // Color calculation
    void updateColor(float dt);
    uint16_t calculateMoodColor(float valence, float arousal);
    
    // Particle count management
    void adjustParticleCount(float dt);
    
    // Rendering
    void renderParticle(const Particle& p);
};

// Global instance
extern ParticleSystem particleSystem;

#endif // PARTICLE_SYSTEM_H
