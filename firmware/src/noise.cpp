/**
 * Ada Particles - Simplex Noise Implementation
 * 
 * Based on FastLED's noise implementation and Stefan Gustavson's
 * simplex noise paper. Optimized for ESP32 with integer math.
 */

#include "noise.h"

// ============================================
// Permutation Table (for pseudo-random gradients)
// ============================================

static uint8_t perm[512];

// Initialize permutation table with seed
void noise_init(uint32_t seed) {
    // Simple LCG to shuffle permutation table based on seed
    uint32_t state = seed;
    
    // Fill first 256 entries with 0-255
    for (int i = 0; i < 256; i++) {
        perm[i] = i;
    }
    
    // Fisher-Yates shuffle
    for (int i = 255; i > 0; i--) {
        state = state * 1664525 + 1013904223; // LCG
        int j = (state >> 16) % (i + 1);
        uint8_t temp = perm[i];
        perm[i] = perm[j];
        perm[j] = temp;
    }
    
    // Duplicate for wrap-around
    for (int i = 0; i < 256; i++) {
        perm[256 + i] = perm[i];
    }
}

// ============================================
// Gradient Tables
// ============================================

// 2D gradients (8 directions)
static const int8_t grad2[8][2] = {
    { 1, 0}, { 1, 1}, { 0, 1}, {-1, 1},
    {-1, 0}, {-1,-1}, { 0,-1}, { 1,-1}
};

// 3D gradients (12 directions)
static const int8_t grad3[12][3] = {
    { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
    { 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
    { 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1}
};

// ============================================
// Helper Functions
// ============================================

// Fast floor for positive values (much faster than floor())
static inline int32_t fastfloor(int32_t x) {
    return x >> 16;
}

// Gradient dot product for 2D
static inline int32_t grad2_dot(int gi, int32_t x, int32_t y) {
    return grad2[gi][0] * x + grad2[gi][1] * y;
}

// Gradient dot product for 3D
static inline int32_t grad3_dot(int gi, int32_t x, int32_t y, int32_t z) {
    return grad3[gi][0] * x + grad3[gi][1] * y + grad3[gi][2] * z;
}

// ============================================
// 2D Simplex Noise
// ============================================

uint16_t noise16_2d(uint32_t x, uint32_t y) {
    // Skew input coordinates for simplex grid
    // F2 = 0.5 * (sqrt(3) - 1) ≈ 0.366025 ≈ 23972 in 16.16
    const int32_t F2 = 23972;
    // G2 = (3 - sqrt(3)) / 6 ≈ 0.211325 ≈ 13853 in 16.16
    const int32_t G2 = 13853;
    
    // Skew
    int32_t s = ((int64_t)(x + y) * F2) >> 16;
    int32_t i = fastfloor(x + s);
    int32_t j = fastfloor(y + s);
    
    // Unskew
    int32_t t = ((int64_t)(i + j) * G2) >> 16;
    int32_t X0 = (i << 16) - t;
    int32_t Y0 = (j << 16) - t;
    
    // Distances from cell origin
    int32_t x0 = x - X0;
    int32_t y0 = y - Y0;
    
    // Determine which simplex we're in
    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }  // Lower triangle
    else         { i1 = 0; j1 = 1; }  // Upper triangle
    
    // Distances from other corners
    int32_t x1 = x0 - (i1 << 16) + G2;
    int32_t y1 = y0 - (j1 << 16) + G2;
    int32_t x2 = x0 - FIXED_ONE + (G2 * 2);
    int32_t y2 = y0 - FIXED_ONE + (G2 * 2);
    
    // Hash coordinates
    int ii = i & 255;
    int jj = j & 255;
    
    // Calculate contributions from corners
    int64_t n0 = 0, n1 = 0, n2 = 0;
    
    // Corner 0
    int32_t t0 = FIXED_HALF - (((int64_t)x0 * x0 + (int64_t)y0 * y0) >> 16);
    if (t0 > 0) {
        t0 = (t0 * t0) >> 16;
        t0 = (t0 * t0) >> 16;
        int gi0 = perm[ii + perm[jj]] & 7;
        n0 = t0 * grad2_dot(gi0, x0 >> 8, y0 >> 8);
    }
    
    // Corner 1
    int32_t t1 = FIXED_HALF - (((int64_t)x1 * x1 + (int64_t)y1 * y1) >> 16);
    if (t1 > 0) {
        t1 = (t1 * t1) >> 16;
        t1 = (t1 * t1) >> 16;
        int gi1 = perm[ii + i1 + perm[jj + j1]] & 7;
        n1 = t1 * grad2_dot(gi1, x1 >> 8, y1 >> 8);
    }
    
    // Corner 2
    int32_t t2 = FIXED_HALF - (((int64_t)x2 * x2 + (int64_t)y2 * y2) >> 16);
    if (t2 > 0) {
        t2 = (t2 * t2) >> 16;
        t2 = (t2 * t2) >> 16;
        int gi2 = perm[ii + 1 + perm[jj + 1]] & 7;
        n2 = t2 * grad2_dot(gi2, x2 >> 8, y2 >> 8);
    }
    
    // Scale and return (target range 0-65535)
    int32_t result = ((n0 + n1 + n2) >> 6) + 32768;
    return (uint16_t)constrain(result, 0, 65535);
}

// ============================================
// 3D Simplex Noise
// ============================================

uint16_t noise16_3d(uint32_t x, uint32_t y, uint32_t z) {
    // Skew factors
    // F3 = 1/3 ≈ 21845 in 16.16
    const int32_t F3 = 21845;
    // G3 = 1/6 ≈ 10923 in 16.16
    const int32_t G3 = 10923;
    
    // Skew input
    int32_t s = ((int64_t)(x + y + z) * F3) >> 16;
    int32_t i = fastfloor(x + s);
    int32_t j = fastfloor(y + s);
    int32_t k = fastfloor(z + s);
    
    // Unskew
    int32_t t = ((int64_t)(i + j + k) * G3) >> 16;
    int32_t X0 = (i << 16) - t;
    int32_t Y0 = (j << 16) - t;
    int32_t Z0 = (k << 16) - t;
    
    // Distances from cell origin
    int32_t x0 = x - X0;
    int32_t y0 = y - Y0;
    int32_t z0 = z - Z0;
    
    // Determine simplex
    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
        else if (x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
        else { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
    } else {
        if (y0 < z0) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
        else if (x0 < z0) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
        else { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
    }
    
    // Distances from other corners
    int32_t x1 = x0 - (i1 << 16) + G3;
    int32_t y1 = y0 - (j1 << 16) + G3;
    int32_t z1 = z0 - (k1 << 16) + G3;
    int32_t x2 = x0 - (i2 << 16) + (G3 * 2);
    int32_t y2 = y0 - (j2 << 16) + (G3 * 2);
    int32_t z2 = z0 - (k2 << 16) + (G3 * 2);
    int32_t x3 = x0 - FIXED_ONE + (G3 * 3);
    int32_t y3 = y0 - FIXED_ONE + (G3 * 3);
    int32_t z3 = z0 - FIXED_ONE + (G3 * 3);
    
    // Hash coordinates
    int ii = i & 255;
    int jj = j & 255;
    int kk = k & 255;
    
    // Calculate contributions
    int64_t n0 = 0, n1 = 0, n2 = 0, n3 = 0;
    
    // Corner 0
    int32_t t0 = (FIXED_HALF * 6 / 10) - (((int64_t)x0 * x0 + (int64_t)y0 * y0 + (int64_t)z0 * z0) >> 16);
    if (t0 > 0) {
        t0 = (t0 * t0) >> 16;
        t0 = (t0 * t0) >> 16;
        int gi0 = perm[ii + perm[jj + perm[kk]]] % 12;
        n0 = t0 * grad3_dot(gi0, x0 >> 8, y0 >> 8, z0 >> 8);
    }
    
    // Corner 1
    int32_t t1 = (FIXED_HALF * 6 / 10) - (((int64_t)x1 * x1 + (int64_t)y1 * y1 + (int64_t)z1 * z1) >> 16);
    if (t1 > 0) {
        t1 = (t1 * t1) >> 16;
        t1 = (t1 * t1) >> 16;
        int gi1 = perm[ii + i1 + perm[jj + j1 + perm[kk + k1]]] % 12;
        n1 = t1 * grad3_dot(gi1, x1 >> 8, y1 >> 8, z1 >> 8);
    }
    
    // Corner 2
    int32_t t2 = (FIXED_HALF * 6 / 10) - (((int64_t)x2 * x2 + (int64_t)y2 * y2 + (int64_t)z2 * z2) >> 16);
    if (t2 > 0) {
        t2 = (t2 * t2) >> 16;
        t2 = (t2 * t2) >> 16;
        int gi2 = perm[ii + i2 + perm[jj + j2 + perm[kk + k2]]] % 12;
        n2 = t2 * grad3_dot(gi2, x2 >> 8, y2 >> 8, z2 >> 8);
    }
    
    // Corner 3
    int32_t t3 = (FIXED_HALF * 6 / 10) - (((int64_t)x3 * x3 + (int64_t)y3 * y3 + (int64_t)z3 * z3) >> 16);
    if (t3 > 0) {
        t3 = (t3 * t3) >> 16;
        t3 = (t3 * t3) >> 16;
        int gi3 = perm[ii + 1 + perm[jj + 1 + perm[kk + 1]]] % 12;
        n3 = t3 * grad3_dot(gi3, x3 >> 8, y3 >> 8, z3 >> 8);
    }
    
    // Scale and return
    int32_t result = ((n0 + n1 + n2 + n3) >> 5) + 32768;
    return (uint16_t)constrain(result, 0, 65535);
}

// ============================================
// Fractal Noise
// ============================================

uint16_t noise16_fractal(uint32_t x, uint32_t y, uint32_t z, uint8_t octaves) {
    int32_t total = 0;
    int32_t maxValue = 0;
    int32_t amplitude = FIXED_ONE;
    uint32_t frequency = FIXED_ONE;
    
    octaves = constrain(octaves, 1, 4);
    
    for (int i = 0; i < octaves; i++) {
        uint32_t sx = (uint32_t)(((int64_t)x * frequency) >> 16);
        uint32_t sy = (uint32_t)(((int64_t)y * frequency) >> 16);
        uint32_t sz = (uint32_t)(((int64_t)z * frequency) >> 16);
        
        int32_t noise = (int32_t)noise16_3d(sx, sy, sz) - 32768;
        total += (noise * amplitude) >> 16;
        
        maxValue += amplitude;
        amplitude >>= 1;  // Halve amplitude (persistence = 0.5)
        frequency <<= 1;  // Double frequency (lacunarity = 2)
    }
    
    // Normalize to 0-65535
    int32_t normalized = ((total << 16) / maxValue) + 32768;
    return (uint16_t)constrain(normalized, 0, 65535);
}

// ============================================
// Curl Noise
// ============================================

void curl_noise_2d(uint32_t x, uint32_t y, uint32_t t, 
                   fixed_t* out_vx, fixed_t* out_vy) {
    // Curl noise creates divergence-free flow
    // curl(x,y) = (dN/dy, -dN/dx)
    // We approximate derivatives with finite differences
    
    const uint32_t eps = 1000;  // Small epsilon for derivative
    
    // Sample noise at offset points
    uint16_t n_px = noise16_3d(x + eps, y, t);
    uint16_t n_mx = noise16_3d(x - eps, y, t);
    uint16_t n_py = noise16_3d(x, y + eps, t);
    uint16_t n_my = noise16_3d(x, y - eps, t);
    
    // Approximate partial derivatives
    int32_t dndx = ((int32_t)n_px - (int32_t)n_mx);
    int32_t dndy = ((int32_t)n_py - (int32_t)n_my);
    
    // Curl: rotate 90 degrees
    *out_vx = dndy;      // dN/dy
    *out_vy = -dndx;     // -dN/dx
}
