#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration
// ============================================
#define WIFI_SSID "TimandAud"
#define WIFI_PASSWORD "Audrianne"

// ============================================
// Ada Server Configuration
// ============================================
#define SERVER_HOST "10.10.10.111"
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
#define TFT_RST   39
#define TFT_SCK   38
#define TFT_D0    4
#define TFT_D1    5
#define TFT_D2    6
#define TFT_D3    7

// I2C Pins
#define IIC_SDA   15
#define IIC_SCL   14

// Touch Pins
#define TP_INT    8
#define TP_RST    -1

// ============================================
// Display Brightness (0-255)
// ============================================
#define DISPLAY_BRIGHTNESS 200

// ============================================
// JPEG Frame Buffer
// Max JPEG frame size we can receive.
// 466x466 JPEG at quality 80 is typically 30-80KB.
// Allocate 150KB to be safe.
// ============================================
#define JPEG_BUFFER_SIZE (150 * 1024)

// ============================================
// WebSocket
// ============================================
#define WS_RECONNECT_INTERVAL_MS 5000

#endif // CONFIG_H
