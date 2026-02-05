/**
 * Ada Particles - Configuration
 * 
 * Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75
 * Display: 466x466 AMOLED with CO5300 driver (QSPI)
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration
// ============================================
#define WIFI_SSID "TimandAud"
#define WIFI_PASSWORD "Audrianne"

// ============================================
// Server Configuration
// ============================================
#define SERVER_HOST "10.10.10.111"
#define SERVER_PORT 8765
#define SERVER_PATH "/"
#define WS_RECONNECT_INTERVAL_MS 5000

// ============================================
// Display Configuration
// ============================================
#define SCREEN_WIDTH 466
#define SCREEN_HEIGHT 466
#define SCREEN_CENTER_X (SCREEN_WIDTH / 2)
#define SCREEN_CENTER_Y (SCREEN_HEIGHT / 2)

// Display rotation: 0=0°, 1=90°CW, 2=180°, 3=270°CW
// USB-C on left (default) = 0
// USB-C on bottom = 3 (270° CW rotation)
#define DISPLAY_ROTATION 3

// Brightness (0-255)
#define DISPLAY_BRIGHTNESS 200
#define DISPLAY_BRIGHTNESS_DISCONNECTED 100

// QSPI Display Pins (CO5300 driver)
#define TFT_CS    12
#define TFT_DC    -1
#define TFT_RST   39
#define TFT_SCK   38
#define TFT_D0    4
#define TFT_D1    5
#define TFT_D2    6
#define TFT_D3    7

// ============================================
// I2C Pins
// ============================================
#define IIC_SDA   15
#define IIC_SCL   14

// ============================================
// Touch Pins (CST9217)
// ============================================
#define TP_INT    8
#define TP_RST    -1

// ============================================
// Particle System Configuration
// ============================================
#define MAX_PARTICLES 400
#define DEFAULT_PARTICLE_COUNT 300

// Particle sizes (diameter in pixels)
#define PARTICLE_SIZE_SMALL   8
#define PARTICLE_SIZE_MEDIUM  16
#define PARTICLE_SIZE_LARGE   24
#define NUM_PARTICLE_SIZES    3

// ============================================
// Physics Tuning
// ============================================

// Noise-based wandering strength (how much particles drift)
#define WANDER_STRENGTH 0.4f

// Attraction toward screen center (prevents drift off-screen)
#define CENTER_PULL 0.0008f

// Spring constant for formation attraction
#define SPRING_K 0.06f

// Velocity damping (0.98 = 2% velocity lost per frame)
#define DAMPING 0.97f

// Maximum velocity (prevents particles flying off)
#define MAX_VELOCITY 8.0f

// ============================================
// Rendering Configuration
// ============================================

// Trail fade factor (0.0-1.0, higher = longer trails)
// 0.92 means each frame retains 92% of previous brightness
#define FADE_FACTOR 0.92f

// Target frame rate
#define TARGET_FPS 30
#define TARGET_FRAME_TIME_MS (1000 / TARGET_FPS)

// ============================================
// Noise Configuration
// ============================================

// Noise scale (smaller = larger, smoother patterns)
#define NOISE_SCALE 0.008f

// Noise time evolution speed
#define NOISE_TIME_SPEED 0.3f

// Number of noise octaves for fractal noise
#define NOISE_OCTAVES 2

// ============================================
// Color Defaults
// ============================================

// Default idle color (cyan)
#define DEFAULT_COLOR_R 0
#define DEFAULT_COLOR_G 255
#define DEFAULT_COLOR_B 204

// Background color (pure black for AMOLED)
#define BG_COLOR 0x0000

// ============================================
// Formation Configuration
// ============================================

// Time to transition between formations (ms)
#define DEFAULT_TRANSITION_MS 2000

// How tightly particles hold formation (0.0-1.0)
// Lower = more "breathing" movement while formed
#define FORMATION_TIGHTNESS 0.7f

// ============================================
// Memory Configuration
// ============================================

// Framebuffer size (466 * 466 * 2 bytes = 434,312 bytes)
#define FRAMEBUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 2)

// ============================================
// Debug Configuration
// ============================================

// Uncomment to enable debug output
#define DEBUG_ENABLED

#ifdef DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// FPS reporting interval (ms)
#define FPS_REPORT_INTERVAL_MS 5000

#endif // CONFIG_H
