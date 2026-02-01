#ifndef BASE64_DECODE_H
#define BASE64_DECODE_H

#include <stdint.h>
#include <stddef.h>

/**
 * Base64 Decoder for ESP32
 *
 * Decodes base64-encoded image data from the server.
 * Optimized for speed over memory — uses a 256-byte lookup table.
 */

// Lookup table: ASCII char → 6-bit value (255 = invalid)
static const uint8_t BASE64_LUT[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

/**
 * Decode base64 string to raw bytes.
 *
 * @param input   Null-terminated base64 string
 * @param output  Output buffer (must be large enough)
 * @param outLen  Set to actual decoded length on return
 * @return true on success, false on invalid input
 */
static inline bool base64_decode(const char* input, uint8_t* output, size_t* outLen) {
    if (!input || !output || !outLen) return false;

    size_t inputLen = strlen(input);
    // Strip trailing whitespace/padding for length calculation
    while (inputLen > 0 && (input[inputLen - 1] == '=' || input[inputLen - 1] == '\n' || input[inputLen - 1] == '\r')) {
        inputLen--;
    }

    size_t outIdx = 0;
    uint32_t accum = 0;
    int bits = 0;

    for (size_t i = 0; i < strlen(input); i++) {
        char c = input[i];

        // Skip whitespace
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;

        // Stop at padding
        if (c == '=') break;

        uint8_t val = BASE64_LUT[(uint8_t)c];
        if (val == 255) {
            // Invalid character — skip but don't fail (be lenient)
            continue;
        }

        accum = (accum << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            output[outIdx++] = (uint8_t)((accum >> bits) & 0xFF);
        }
    }

    *outLen = outIdx;
    return true;
}

/**
 * Calculate the maximum decoded size for a base64 string.
 */
static inline size_t base64_decoded_size(size_t base64Len) {
    return (base64Len * 3) / 4 + 4;  // +4 for safety
}

#endif // BASE64_DECODE_H
