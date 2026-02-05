/**
 * Ada Particles - Sprite Generation Implementation
 */

#include "sprites.h"
#include <math.h>

// Global instance
ParticleSprites particleSprites;

// ============================================
// Constructor / Destructor
// ============================================

ParticleSprites::ParticleSprites() 
    : _ready(false), _memoryUsed(0) {
    memset(_sprites, 0, sizeof(_sprites));
    _sizes[0] = PARTICLE_SIZE_SMALL;   // 8px
    _sizes[1] = PARTICLE_SIZE_MEDIUM;  // 16px
    _sizes[2] = PARTICLE_SIZE_LARGE;   // 24px
}

ParticleSprites::~ParticleSprites() {
    for (int i = 0; i < NUM_PARTICLE_SIZES; i++) {
        if (_sprites[i]) {
            free(_sprites[i]);
            _sprites[i] = nullptr;
        }
    }
}

// ============================================
// Generation
// ============================================

bool ParticleSprites::generate() {
    Serial.println("Generating particle sprites...");
    
    // Sigma values for each size (controls softness)
    // Smaller sigma = sharper center, faster falloff
    // Larger sigma = softer overall
    float sigmas[3] = {
        2.5f,   // Small: tighter glow
        4.0f,   // Medium: balanced
        6.0f    // Large: very soft
    };
    
    _memoryUsed = 0;
    
    for (int i = 0; i < NUM_PARTICLE_SIZES; i++) {
        _sprites[i] = generateSprite(_sizes[i], sigmas[i]);
        
        if (!_sprites[i]) {
            Serial.printf("ERROR: Failed to generate sprite %d\n", i);
            return false;
        }
        
        size_t spriteBytes = _sizes[i] * _sizes[i];
        _memoryUsed += spriteBytes;
        
        Serial.printf("  Sprite %d: %dx%d (%d bytes)\n", 
                      i, _sizes[i], _sizes[i], spriteBytes);
    }
    
    Serial.printf("Sprites ready: %d bytes total\n", _memoryUsed);
    _ready = true;
    return true;
}

uint8_t* ParticleSprites::generateSprite(uint8_t diameter, float sigma) {
    size_t pixels = diameter * diameter;
    
    // Allocate in PSRAM
    uint8_t* sprite = (uint8_t*)ps_malloc(pixels);
    if (!sprite) {
        sprite = (uint8_t*)malloc(pixels);
    }
    if (!sprite) return nullptr;
    
    float radius = diameter / 2.0f;
    float centerX = radius - 0.5f;
    float centerY = radius - 0.5f;
    
    // Pre-calculate Gaussian coefficient
    // G(d) = exp(-d²/(2σ²))
    float sigma2 = 2.0f * sigma * sigma;
    
    for (int y = 0; y < diameter; y++) {
        float dy = y - centerY;
        
        for (int x = 0; x < diameter; x++) {
            float dx = x - centerX;
            
            // Distance from center
            float distSq = dx * dx + dy * dy;
            float dist = sqrtf(distSq);
            
            // Gaussian falloff
            float intensity = expf(-distSq / sigma2);
            
            // Apply soft edge at sprite boundary
            // This prevents hard circular edges
            float edgeDist = radius - dist;
            if (edgeDist < 0) {
                intensity = 0;
            } else if (edgeDist < 1.5f) {
                // Smooth fade at edge
                intensity *= edgeDist / 1.5f;
            }
            
            // Clamp and convert to 8-bit
            intensity = fmaxf(0.0f, fminf(1.0f, intensity));
            sprite[y * diameter + x] = (uint8_t)(intensity * 255.0f);
        }
    }
    
    return sprite;
}

// ============================================
// Accessors
// ============================================

const uint8_t* ParticleSprites::getSprite(uint8_t sizeIdx) const {
    if (sizeIdx >= NUM_PARTICLE_SIZES || !_ready) return nullptr;
    return _sprites[sizeIdx];
}

uint8_t ParticleSprites::getSpriteSize(uint8_t sizeIdx) const {
    if (sizeIdx >= NUM_PARTICLE_SIZES) return 0;
    return _sizes[sizeIdx];
}
