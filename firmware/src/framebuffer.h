/**
 * Ada Particles - Framebuffer with Fade Trail Effect
 * 
 * PSRAM-backed framebuffer for double-buffering and smooth trails.
 * Instead of clearing to black each frame, we fade the existing
 * content, creating dreamy particle trails.
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../config.h"

// ============================================
// Framebuffer Class
// ============================================

class Framebuffer {
public:
    Framebuffer();
    ~Framebuffer();
    
    /**
     * Initialize framebuffer in PSRAM.
     * @param gfx Pointer to display for pushing frames
     * @return true if allocation succeeded
     */
    bool init(Arduino_GFX* gfx);
    
    /**
     * Clear framebuffer to solid color.
     * @param color RGB565 color (default: black)
     */
    void clear(uint16_t color = 0x0000);
    
    /**
     * Fade framebuffer toward black.
     * Each pixel's RGB channels are multiplied by factor.
     * This creates the trail effect.
     * 
     * @param factor Fade factor 0.0-1.0 (0.92 = retain 92%)
     */
    void fade(float factor);
    
    /**
     * Fast fade using integer math.
     * @param factor256 Fade factor 0-256 (236 ≈ 0.92)
     */
    void fadeFast(uint8_t factor256);
    
    /**
     * Draw a single pixel.
     * @param x X coordinate
     * @param y Y coordinate
     * @param color RGB565 color
     */
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    
    /**
     * Draw pixel with additive blending.
     * New color is added to existing, clamped to white.
     * Creates glowing effect when particles overlap.
     * 
     * @param x X coordinate
     * @param y Y coordinate
     * @param color RGB565 color to add
     */
    void drawPixelAdditive(int16_t x, int16_t y, uint16_t color);
    
    /**
     * Draw pixel with additive blending and brightness.
     * @param x X coordinate
     * @param y Y coordinate
     * @param color RGB565 base color
     * @param brightness 0-255 brightness multiplier
     */
    void drawPixelAdditiveBright(int16_t x, int16_t y, uint16_t color, uint8_t brightness);
    
    /**
     * Draw filled circle.
     * @param cx Center X
     * @param cy Center Y
     * @param radius Radius
     * @param color RGB565 color
     */
    void fillCircle(int16_t cx, int16_t cy, int16_t radius, uint16_t color);
    
    /**
     * Draw filled circle with additive blending.
     */
    void fillCircleAdditive(int16_t cx, int16_t cy, int16_t radius, 
                           uint16_t color, uint8_t brightness = 255);
    
    /**
     * Draw a soft/anti-aliased circle from pre-rendered sprite.
     * This is the main particle rendering function.
     * 
     * @param cx Center X
     * @param cy Center Y
     * @param spriteIdx Which pre-rendered sprite to use (0-2)
     * @param color RGB565 base color
     * @param brightness 0-255 brightness
     */
    void drawSoftParticle(int16_t cx, int16_t cy, uint8_t spriteIdx,
                          uint16_t color, uint8_t brightness);
    
    /**
     * Set pointer to particle sprites for soft rendering.
     * @param sprites Array of 3 sprite pointers (small, medium, large)
     * @param sizes Array of sprite diameters
     */
    void setParticleSprites(const uint8_t** sprites, const uint8_t* sizes);
    
    /**
     * Push framebuffer to display.
     * Uses efficient block transfer.
     */
    void pushToDisplay();
    
    /**
     * Get direct access to buffer (for advanced rendering).
     */
    uint16_t* getBuffer() { return _buffer; }
    
    /**
     * Get pixel at coordinates.
     */
    uint16_t getPixel(int16_t x, int16_t y);
    
    // Accessors
    int16_t width() const { return SCREEN_WIDTH; }
    int16_t height() const { return SCREEN_HEIGHT; }
    bool isValid() const { return _buffer != nullptr; }

private:
    uint16_t* _buffer;          // RGB565 framebuffer in PSRAM
    Arduino_GFX* _gfx;          // Display pointer
    
    // Particle sprite pointers
    const uint8_t* _sprites[3];
    uint8_t _spriteSizes[3];
    bool _spritesSet;
    
    // Helper: Check bounds
    inline bool inBounds(int16_t x, int16_t y) const {
        return x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT;
    }
    
    // Helper: Get buffer index
    inline size_t bufferIndex(int16_t x, int16_t y) const {
        return (size_t)y * SCREEN_WIDTH + x;
    }
    
    // Helper: RGB565 channel extraction
    static inline uint8_t rgb565_r(uint16_t c) { return (c >> 11) & 0x1F; }
    static inline uint8_t rgb565_g(uint16_t c) { return (c >> 5) & 0x3F; }
    static inline uint8_t rgb565_b(uint16_t c) { return c & 0x1F; }
    
    // Helper: RGB565 composition
    static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
    }
    
    // Helper: Convert 8-bit to 5-bit
    static inline uint8_t to5bit(uint8_t v) { return v >> 3; }
    
    // Helper: Convert 8-bit to 6-bit
    static inline uint8_t to6bit(uint8_t v) { return v >> 2; }
};

#endif // FRAMEBUFFER_H
