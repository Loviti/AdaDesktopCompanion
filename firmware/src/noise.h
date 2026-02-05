/**
 * Ada Particles - Simplex Noise Implementation
 * 
 * 16-bit simplex noise for organic particle motion.
 * Based on FastLED's noise implementation, adapted for our needs.
 * 
 * Input coordinates are 16.16 fixed-point.
 * Output is 0-65535 (center around 32768 for signed use).
 */

#ifndef NOISE_H
#define NOISE_H

#include <Arduino.h>
#include "fixed_math.h"

// ============================================
// Noise Functions
// ============================================

/**
 * 2D Simplex noise.
 * 
 * @param x X coordinate (16.16 fixed-point)
 * @param y Y coordinate (16.16 fixed-point)
 * @return Noise value 0-65535
 */
uint16_t noise16_2d(uint32_t x, uint32_t y);

/**
 * 3D Simplex noise (useful for animated 2D noise: x, y, time).
 * 
 * @param x X coordinate (16.16 fixed-point)
 * @param y Y coordinate (16.16 fixed-point)
 * @param z Z coordinate / time (16.16 fixed-point)
 * @return Noise value 0-65535
 */
uint16_t noise16_3d(uint32_t x, uint32_t y, uint32_t z);

/**
 * Fractal/octave noise (2D with time).
 * Combines multiple octaves for richer detail.
 * 
 * @param x X coordinate (16.16 fixed-point)
 * @param y Y coordinate (16.16 fixed-point)
 * @param z Time/Z coordinate (16.16 fixed-point)
 * @param octaves Number of octaves to combine (1-4)
 * @return Noise value 0-65535
 */
uint16_t noise16_fractal(uint32_t x, uint32_t y, uint32_t z, uint8_t octaves);

/**
 * Get signed noise value (-32768 to +32767).
 * Useful for velocity offsets.
 */
static inline int16_t noise16_signed(uint32_t x, uint32_t y, uint32_t z) {
    return (int16_t)(noise16_3d(x, y, z) - 32768);
}

/**
 * Get noise value as fixed-point (-1.0 to +1.0).
 * Returns value in 16.16 fixed-point format.
 */
static inline fixed_t noise16_fixed(uint32_t x, uint32_t y, uint32_t z) {
    // Convert 0-65535 to -32768 to +32767, then scale
    int16_t s = (int16_t)(noise16_3d(x, y, z) - 32768);
    // s is -32768 to 32767, we want -65536 to 65536 (fixed-point -1 to 1)
    return (fixed_t)s * 2;
}

/**
 * Get 2D curl noise for fluid-like motion.
 * Returns velocity components that create swirling, divergence-free flow.
 * 
 * @param x X coordinate (16.16 fixed-point)
 * @param y Y coordinate (16.16 fixed-point)
 * @param t Time (16.16 fixed-point)
 * @param out_vx Output X velocity component (fixed-point)
 * @param out_vy Output Y velocity component (fixed-point)
 */
void curl_noise_2d(uint32_t x, uint32_t y, uint32_t t, 
                   fixed_t* out_vx, fixed_t* out_vy);

// ============================================
// Initialization
// ============================================

/**
 * Initialize noise system (call once at startup).
 * Seeds the permutation table.
 */
void noise_init(uint32_t seed);

#endif // NOISE_H
