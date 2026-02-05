/**
 * Ada Particles - Framebuffer Implementation
 */

#include "framebuffer.h"

// ============================================
// Constructor / Destructor
// ============================================

Framebuffer::Framebuffer() 
    : _buffer(nullptr), _gfx(nullptr), _spritesSet(false) {
    memset(_sprites, 0, sizeof(_sprites));
    memset(_spriteSizes, 0, sizeof(_spriteSizes));
}

Framebuffer::~Framebuffer() {
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
}

// ============================================
// Initialization
// ============================================

bool Framebuffer::init(Arduino_GFX* gfx) {
    _gfx = gfx;
    
    // Allocate framebuffer in PSRAM
    size_t bufferBytes = FRAMEBUFFER_SIZE;
    
    _buffer = (uint16_t*)ps_malloc(bufferBytes);
    if (!_buffer) {
        // Fallback to regular malloc (will likely fail for large buffer)
        _buffer = (uint16_t*)malloc(bufferBytes);
    }
    
    if (!_buffer) {
        Serial.println("ERROR: Failed to allocate framebuffer!");
        return false;
    }
    
    Serial.printf("Framebuffer allocated: %d bytes (%d x %d)\n", 
                  bufferBytes, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Clear to black
    clear(0x0000);
    
    return true;
}

// ============================================
// Clear / Fade
// ============================================

void Framebuffer::clear(uint16_t color) {
    if (!_buffer) return;
    
    size_t pixels = SCREEN_WIDTH * SCREEN_HEIGHT;
    
    // Fast clear using 32-bit writes
    uint32_t color32 = ((uint32_t)color << 16) | color;
    uint32_t* buf32 = (uint32_t*)_buffer;
    size_t words = pixels / 2;
    
    for (size_t i = 0; i < words; i++) {
        buf32[i] = color32;
    }
    
    // Handle odd pixel if any
    if (pixels & 1) {
        _buffer[pixels - 1] = color;
    }
}

void Framebuffer::fade(float factor) {
    // Convert to 8-bit integer (0-256 range for faster math)
    uint8_t factor256 = (uint8_t)(factor * 256.0f);
    fadeFast(factor256);
}

void Framebuffer::fadeFast(uint8_t factor256) {
    if (!_buffer) return;
    
    size_t pixels = SCREEN_WIDTH * SCREEN_HEIGHT;
    
    // Process in batches for cache efficiency
    for (size_t i = 0; i < pixels; i++) {
        uint16_t c = _buffer[i];
        
        if (c == 0) continue;  // Skip pure black (common case)
        
        // Extract RGB565 components
        uint8_t r = rgb565_r(c);  // 5 bits (0-31)
        uint8_t g = rgb565_g(c);  // 6 bits (0-63)
        uint8_t b = rgb565_b(c);  // 5 bits (0-31)
        
        // Apply fade (integer multiply, then shift)
        // factor256 is 0-256, so multiply and shift by 8
        r = (r * factor256) >> 8;
        g = (g * factor256) >> 8;
        b = (b * factor256) >> 8;
        
        _buffer[i] = rgb565(r, g, b);
    }
}

// ============================================
// Pixel Drawing
// ============================================

void Framebuffer::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!_buffer || !inBounds(x, y)) return;
    _buffer[bufferIndex(x, y)] = color;
}

void Framebuffer::drawPixelAdditive(int16_t x, int16_t y, uint16_t color) {
    if (!_buffer || !inBounds(x, y)) return;
    
    size_t idx = bufferIndex(x, y);
    uint16_t existing = _buffer[idx];
    
    // Extract channels
    uint8_t er = rgb565_r(existing);
    uint8_t eg = rgb565_g(existing);
    uint8_t eb = rgb565_b(existing);
    
    uint8_t nr = rgb565_r(color);
    uint8_t ng = rgb565_g(color);
    uint8_t nb = rgb565_b(color);
    
    // Add and clamp
    uint8_t fr = min(31, er + nr);
    uint8_t fg = min(63, eg + ng);
    uint8_t fb = min(31, eb + nb);
    
    _buffer[idx] = rgb565(fr, fg, fb);
}

void Framebuffer::drawPixelAdditiveBright(int16_t x, int16_t y, 
                                          uint16_t color, uint8_t brightness) {
    if (!_buffer || !inBounds(x, y)) return;
    
    size_t idx = bufferIndex(x, y);
    uint16_t existing = _buffer[idx];
    
    // Extract and scale new color by brightness
    uint8_t nr = (rgb565_r(color) * brightness) >> 8;
    uint8_t ng = (rgb565_g(color) * brightness) >> 8;
    uint8_t nb = (rgb565_b(color) * brightness) >> 8;
    
    // Extract existing
    uint8_t er = rgb565_r(existing);
    uint8_t eg = rgb565_g(existing);
    uint8_t eb = rgb565_b(existing);
    
    // Add and clamp
    _buffer[idx] = rgb565(
        min(31, er + nr),
        min(63, eg + ng),
        min(31, eb + nb)
    );
}

uint16_t Framebuffer::getPixel(int16_t x, int16_t y) {
    if (!_buffer || !inBounds(x, y)) return 0;
    return _buffer[bufferIndex(x, y)];
}

// ============================================
// Circle Drawing
// ============================================

void Framebuffer::fillCircle(int16_t cx, int16_t cy, int16_t radius, uint16_t color) {
    if (!_buffer) return;
    
    int16_t x = 0;
    int16_t y = radius;
    int16_t d = 3 - 2 * radius;
    
    auto drawHLine = [&](int16_t x1, int16_t x2, int16_t y) {
        if (y < 0 || y >= SCREEN_HEIGHT) return;
        if (x1 > x2) { int16_t t = x1; x1 = x2; x2 = t; }
        x1 = max((int16_t)0, x1);
        x2 = min((int16_t)(SCREEN_WIDTH - 1), x2);
        for (int16_t px = x1; px <= x2; px++) {
            _buffer[bufferIndex(px, y)] = color;
        }
    };
    
    while (y >= x) {
        // Draw horizontal lines for filled circle
        drawHLine(cx - x, cx + x, cy - y);
        drawHLine(cx - x, cx + x, cy + y);
        drawHLine(cx - y, cx + y, cy - x);
        drawHLine(cx - y, cx + y, cy + x);
        
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void Framebuffer::fillCircleAdditive(int16_t cx, int16_t cy, int16_t radius,
                                     uint16_t color, uint8_t brightness) {
    if (!_buffer) return;
    
    // Pre-scale color by brightness
    uint8_t nr = (rgb565_r(color) * brightness) >> 8;
    uint8_t ng = (rgb565_g(color) * brightness) >> 8;
    uint8_t nb = (rgb565_b(color) * brightness) >> 8;
    
    int16_t x = 0;
    int16_t y = radius;
    int16_t d = 3 - 2 * radius;
    
    auto drawHLineAdd = [&](int16_t x1, int16_t x2, int16_t y) {
        if (y < 0 || y >= SCREEN_HEIGHT) return;
        if (x1 > x2) { int16_t t = x1; x1 = x2; x2 = t; }
        x1 = max((int16_t)0, x1);
        x2 = min((int16_t)(SCREEN_WIDTH - 1), x2);
        
        for (int16_t px = x1; px <= x2; px++) {
            size_t idx = bufferIndex(px, y);
            uint16_t existing = _buffer[idx];
            
            uint8_t er = rgb565_r(existing);
            uint8_t eg = rgb565_g(existing);
            uint8_t eb = rgb565_b(existing);
            
            _buffer[idx] = rgb565(
                min(31, er + nr),
                min(63, eg + ng),
                min(31, eb + nb)
            );
        }
    };
    
    while (y >= x) {
        drawHLineAdd(cx - x, cx + x, cy - y);
        drawHLineAdd(cx - x, cx + x, cy + y);
        drawHLineAdd(cx - y, cx + y, cy - x);
        drawHLineAdd(cx - y, cx + y, cy + x);
        
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

// ============================================
// Soft Particle Rendering
// ============================================

void Framebuffer::setParticleSprites(const uint8_t** sprites, const uint8_t* sizes) {
    for (int i = 0; i < 3; i++) {
        _sprites[i] = sprites[i];
        _spriteSizes[i] = sizes[i];
    }
    _spritesSet = true;
}

void Framebuffer::drawSoftParticle(int16_t cx, int16_t cy, uint8_t spriteIdx,
                                   uint16_t color, uint8_t brightness) {
    if (!_buffer || !_spritesSet) return;
    if (spriteIdx >= 3) spriteIdx = 2;
    
    const uint8_t* sprite = _sprites[spriteIdx];
    uint8_t size = _spriteSizes[spriteIdx];
    int16_t halfSize = size / 2;
    
    if (!sprite) return;
    
    // Pre-calculate color components
    uint8_t baseR = rgb565_r(color);
    uint8_t baseG = rgb565_g(color);
    uint8_t baseB = rgb565_b(color);
    
    // Iterate over sprite pixels
    for (int16_t sy = 0; sy < size; sy++) {
        int16_t screenY = cy - halfSize + sy;
        if (screenY < 0 || screenY >= SCREEN_HEIGHT) continue;
        
        for (int16_t sx = 0; sx < size; sx++) {
            int16_t screenX = cx - halfSize + sx;
            if (screenX < 0 || screenX >= SCREEN_WIDTH) continue;
            
            // Get sprite alpha value
            uint8_t alpha = sprite[sy * size + sx];
            if (alpha == 0) continue;
            
            // Combine alpha with brightness
            uint16_t combinedAlpha = ((uint16_t)alpha * brightness) >> 8;
            if (combinedAlpha == 0) continue;
            
            // Calculate final color contribution
            uint8_t nr = (baseR * combinedAlpha) >> 8;
            uint8_t ng = (baseG * combinedAlpha) >> 8;
            uint8_t nb = (baseB * combinedAlpha) >> 8;
            
            // Additive blend to framebuffer
            size_t idx = bufferIndex(screenX, screenY);
            uint16_t existing = _buffer[idx];
            
            _buffer[idx] = rgb565(
                min(31, (int)rgb565_r(existing) + nr),
                min(63, (int)rgb565_g(existing) + ng),
                min(31, (int)rgb565_b(existing) + nb)
            );
        }
    }
}

// ============================================
// Display Output
// ============================================

void Framebuffer::pushToDisplay() {
    if (!_buffer || !_gfx) return;
    
    // Use efficient bitmap draw function
    _gfx->draw16bitRGBBitmap(0, 0, _buffer, SCREEN_WIDTH, SCREEN_HEIGHT);
}
