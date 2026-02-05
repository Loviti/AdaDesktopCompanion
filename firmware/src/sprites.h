/**
 * Ada Particles - Pre-rendered Soft Particle Sprites
 * 
 * Generates Gaussian-falloff circular sprites at boot time.
 * These provide the soft, anti-aliased particle look without
 * expensive per-pixel calculations during rendering.
 */

#ifndef SPRITES_H
#define SPRITES_H

#include <Arduino.h>
#include "../config.h"

// ============================================
// Sprite System
// ============================================

class ParticleSprites {
public:
    ParticleSprites();
    ~ParticleSprites();
    
    /**
     * Generate particle sprites in PSRAM.
     * Call once at startup after PSRAM is available.
     * @return true if allocation and generation succeeded
     */
    bool generate();
    
    /**
     * Get pointer to a sprite's alpha data.
     * @param sizeIdx 0=small, 1=medium, 2=large
     * @return Pointer to sprite alpha map, or nullptr
     */
    const uint8_t* getSprite(uint8_t sizeIdx) const;
    
    /**
     * Get sprite diameter.
     * @param sizeIdx 0=small, 1=medium, 2=large
     * @return Sprite diameter in pixels
     */
    uint8_t getSpriteSize(uint8_t sizeIdx) const;
    
    /**
     * Get array of all sprite pointers (for Framebuffer).
     */
    const uint8_t** getSpriteArray() const { return (const uint8_t**)_sprites; }
    
    /**
     * Get array of all sprite sizes (for Framebuffer).
     */
    const uint8_t* getSizesArray() const { return _sizes; }
    
    /**
     * Check if sprites are ready.
     */
    bool isReady() const { return _ready; }
    
    /**
     * Get total memory used by sprites.
     */
    size_t getMemoryUsage() const { return _memoryUsed; }

private:
    uint8_t* _sprites[NUM_PARTICLE_SIZES];
    uint8_t _sizes[NUM_PARTICLE_SIZES];
    bool _ready;
    size_t _memoryUsed;
    
    /**
     * Generate a single sprite with Gaussian falloff.
     * @param diameter Sprite diameter in pixels
     * @param sigma Gaussian sigma (controls softness)
     * @return Pointer to allocated sprite, or nullptr
     */
    uint8_t* generateSprite(uint8_t diameter, float sigma);
};

// Global instance
extern ParticleSprites particleSprites;

#endif // SPRITES_H
