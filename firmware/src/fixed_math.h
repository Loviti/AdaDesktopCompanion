/**
 * Ada Particles - Fixed-Point Math Utilities
 * 
 * 16.16 fixed-point arithmetic for fast particle physics on ESP32.
 * Integer part in high 16 bits, fractional part in low 16 bits.
 * 
 * Range: -32768.0 to +32767.99998 (approximately)
 * Resolution: 1/65536 ≈ 0.000015
 */

#ifndef FIXED_MATH_H
#define FIXED_MATH_H

#include <Arduino.h>

// ============================================
// Fixed-Point Type Definition
// ============================================

typedef int32_t fixed_t;

// Number of fractional bits
#define FIXED_SHIFT 16
#define FIXED_ONE (1 << FIXED_SHIFT)
#define FIXED_HALF (1 << (FIXED_SHIFT - 1))

// ============================================
// Conversion Macros
// ============================================

// Integer to fixed
#define INT_TO_FIXED(i) ((fixed_t)(i) << FIXED_SHIFT)

// Float to fixed (use sparingly - slow!)
#define FLOAT_TO_FIXED(f) ((fixed_t)((f) * (float)FIXED_ONE))

// Fixed to integer (truncates)
#define FIXED_TO_INT(f) ((int32_t)(f) >> FIXED_SHIFT)

// Fixed to integer (rounds)
#define FIXED_TO_INT_ROUND(f) ((int32_t)((f) + FIXED_HALF) >> FIXED_SHIFT)

// Fixed to float (use sparingly - slow!)
#define FIXED_TO_FLOAT(f) ((float)(f) / (float)FIXED_ONE)

// ============================================
// Basic Arithmetic
// ============================================

// Addition and subtraction work directly
// fixed_t sum = a + b;
// fixed_t diff = a - b;

/**
 * Fixed-point multiplication.
 * Result = (a * b) >> 16
 * Uses 64-bit intermediate to prevent overflow.
 */
static inline fixed_t fixed_mul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * (int64_t)b) >> FIXED_SHIFT);
}

/**
 * Fixed-point division.
 * Result = (a << 16) / b
 * Uses 64-bit intermediate to prevent overflow.
 */
static inline fixed_t fixed_div(fixed_t a, fixed_t b) {
    if (b == 0) return (a >= 0) ? INT32_MAX : INT32_MIN;
    return (fixed_t)(((int64_t)a << FIXED_SHIFT) / (int64_t)b);
}

/**
 * Absolute value.
 */
static inline fixed_t fixed_abs(fixed_t x) {
    return (x < 0) ? -x : x;
}

/**
 * Minimum of two values.
 */
static inline fixed_t fixed_min(fixed_t a, fixed_t b) {
    return (a < b) ? a : b;
}

/**
 * Maximum of two values.
 */
static inline fixed_t fixed_max(fixed_t a, fixed_t b) {
    return (a > b) ? a : b;
}

/**
 * Clamp value to range.
 */
static inline fixed_t fixed_clamp(fixed_t x, fixed_t min_val, fixed_t max_val) {
    if (x < min_val) return min_val;
    if (x > max_val) return max_val;
    return x;
}

// ============================================
// Trigonometry (Lookup Tables)
// ============================================

// Sine table: 256 entries for 0-90 degrees (quarter wave)
// Values are in 16.16 fixed-point, scaled to range [0, 65536] for sin(0) to sin(90)
// Full sine wave reconstructed by symmetry

// Table size (entries per quadrant)
#define TRIG_TABLE_SIZE 256
#define TRIG_TABLE_SHIFT 8  // log2(256)

// Pre-computed sine table (quarter wave: 0 to 90 degrees)
// Generated values: sin(i * PI/2 / 256) * 65536
extern const int16_t SIN_TABLE[TRIG_TABLE_SIZE + 1];

/**
 * Fast sine using lookup table.
 * Input: angle in fixed-point where 1.0 = 2*PI (full circle)
 *        i.e., 0x10000 = 360 degrees
 * Output: sine value in 16.16 fixed-point [-1.0, 1.0]
 */
static inline fixed_t fixed_sin(fixed_t angle) {
    // Normalize angle to [0, 0xFFFF]
    uint16_t a = (uint16_t)(angle & 0xFFFF);
    
    // Determine quadrant and table index
    // a is 0-65535, representing 0-360 degrees
    // Quadrant: 0=0-90, 1=90-180, 2=180-270, 3=270-360
    uint8_t quadrant = a >> 14;  // Top 2 bits
    uint16_t index = a & 0x3FFF; // Bottom 14 bits
    
    // Scale index to table size (0-255)
    // 14 bits -> 8 bits: shift right by 6
    uint8_t tableIdx = index >> 6;
    
    // Get value based on quadrant
    int32_t value;
    switch (quadrant) {
        case 0: // 0-90: direct lookup
            value = SIN_TABLE[tableIdx];
            break;
        case 1: // 90-180: mirror
            value = SIN_TABLE[TRIG_TABLE_SIZE - tableIdx];
            break;
        case 2: // 180-270: negate
            value = -SIN_TABLE[tableIdx];
            break;
        case 3: // 270-360: mirror and negate
            value = -SIN_TABLE[TRIG_TABLE_SIZE - tableIdx];
            break;
    }
    
    return value;
}

/**
 * Fast cosine using lookup table.
 * cos(x) = sin(x + PI/2)
 */
static inline fixed_t fixed_cos(fixed_t angle) {
    // Add 90 degrees (0x4000 in our 16-bit angle system)
    return fixed_sin(angle + 0x4000);
}

// ============================================
// Square Root (Fast Approximation)
// ============================================

/**
 * Fast integer square root using binary search.
 * Input: non-negative fixed-point value
 * Output: square root as fixed-point
 */
static inline fixed_t fixed_sqrt(fixed_t x) {
    if (x <= 0) return 0;
    
    // Use 64-bit to handle large values
    // sqrt(x) in fixed point = sqrt(x * 65536) / 256 = sqrt(x) * sqrt(65536) / 256 = sqrt(x) * 256 / 256
    // Actually: sqrt of a 16.16 fixed number needs adjustment
    // If x represents X in fixed (x = X * 65536), then sqrt(x) should represent sqrt(X)
    // sqrt(x) = sqrt(X * 65536) = sqrt(X) * 256
    // So we need: sqrt(x * 65536) which gives us sqrt(X) * 65536
    
    uint64_t val = (uint64_t)x << FIXED_SHIFT;
    uint64_t result = 0;
    uint64_t bit = (uint64_t)1 << 62;
    
    // Find highest bit
    while (bit > val) bit >>= 2;
    
    // Binary search
    while (bit != 0) {
        if (val >= result + bit) {
            val -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    
    return (fixed_t)result;
}

// ============================================
// Linear Interpolation
// ============================================

/**
 * Linear interpolation between two fixed-point values.
 * t should be in range [0, FIXED_ONE]
 */
static inline fixed_t fixed_lerp(fixed_t a, fixed_t b, fixed_t t) {
    return a + fixed_mul(b - a, t);
}

/**
 * Smooth step (cubic Hermite interpolation).
 * t should be in range [0, FIXED_ONE]
 * Returns smooth transition from 0 to 1.
 */
static inline fixed_t fixed_smoothstep(fixed_t t) {
    if (t <= 0) return 0;
    if (t >= FIXED_ONE) return FIXED_ONE;
    // 3t² - 2t³
    fixed_t t2 = fixed_mul(t, t);
    fixed_t t3 = fixed_mul(t2, t);
    return fixed_mul(INT_TO_FIXED(3), t2) - fixed_mul(INT_TO_FIXED(2), t3);
}

// ============================================
// Distance Calculations
// ============================================

/**
 * Squared distance between two points.
 * Returns fixed-point squared distance (no sqrt needed for comparisons).
 */
static inline fixed_t fixed_dist_sq(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2) {
    fixed_t dx = x2 - x1;
    fixed_t dy = y2 - y1;
    return fixed_mul(dx, dx) + fixed_mul(dy, dy);
}

/**
 * Distance between two points.
 * Warning: involves sqrt, use dist_sq when possible.
 */
static inline fixed_t fixed_dist(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2) {
    return fixed_sqrt(fixed_dist_sq(x1, y1, x2, y2));
}

// ============================================
// Constants
// ============================================

// PI in 16.16 fixed-point (3.14159... * 65536 = 205887)
#define FIXED_PI 205887

// 2*PI
#define FIXED_TWO_PI 411775

// PI/2
#define FIXED_HALF_PI 102944

// Common fractions
#define FIXED_TENTH 6554      // 0.1 * 65536
#define FIXED_QUARTER 16384   // 0.25 * 65536
#define FIXED_THIRD 21845     // 0.333... * 65536
#define FIXED_HALF 32768      // 0.5 * 65536
#define FIXED_TWO_THIRDS 43691

#endif // FIXED_MATH_H
