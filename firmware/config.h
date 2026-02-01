#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration
// ============================================
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ============================================
// Ada Server Configuration
// ============================================
#define SERVER_HOST "10.10.10.100"
#define SERVER_PORT 8765
#define SERVER_PATH "/"

// ============================================
// Display Configuration (Waveshare ESP32-S3-Touch-AMOLED-1.75)
// ============================================
#define SCREEN_WIDTH 466
#define SCREEN_HEIGHT 466

// QSPI Display Pins (CO5300 driver)
#define TFT_CS    12
#define TFT_DC    -1
#define TFT_RST   17
#define TFT_SCK   47
#define TFT_D0    18
#define TFT_D1    7
#define TFT_D2    48
#define TFT_D3    5

// I2C Pins
#define IIC_SDA   6
#define IIC_SCL   4

// Touch Pins
#define TP_INT    8
#define TP_RST    -1

// ============================================
// Particle System
// ============================================
#define MAX_PARTICLES 1500
#define DEFAULT_PARTICLE_COUNT 800
#define DEFAULT_PARTICLE_SIZE 3.0f
#define DEFAULT_PARTICLE_SPEED 1.0f
#define DEFAULT_DISPERSION 30.0f
#define DEFAULT_OPACITY 1.0f
#define DEFAULT_PULSE_SPEED 1.0f
#define DEFAULT_ROTATION_SPEED 0.0f

// ============================================
// Animation
// ============================================
#define TARGET_FPS 30
#define FRAME_TIME_MS (1000 / TARGET_FPS)
#define CONFIG_LERP_SPEED 3.0f       // How fast config transitions happen (per second)
#define POSITION_LERP_SPEED 2.0f     // How fast particles morph to new positions
#define FADE_OUT_SPEED 2.0f          // Opacity decrease per second during clear

// ============================================
// Image Buffer
// ============================================
#define MAX_IMAGE_WIDTH 256
#define MAX_IMAGE_HEIGHT 256
#define MAX_IMAGE_BYTES (MAX_IMAGE_WIDTH * MAX_IMAGE_HEIGHT * 3)
// Base64 of max image: ceil(MAX_IMAGE_BYTES * 4/3)
#define MAX_BASE64_SIZE ((MAX_IMAGE_BYTES + 2) / 3 * 4 + 1)

// ============================================
// WebSocket
// ============================================
#define WS_RECONNECT_INTERVAL_MS 5000
// JSON doc size: enough for base64 image + config
// 128x128x3 = 49152 bytes, base64 = ~65536 chars, plus JSON overhead
#define JSON_DOC_SIZE (128 * 1024)

// ============================================
// Display Brightness
// ============================================
#define DISPLAY_BRIGHTNESS 200  // 0-255

#endif // CONFIG_H
