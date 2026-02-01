/**
 * Ada Particles — ESP32 Belly Display Firmware (Streaming Mode)
 *
 * Minimal firmware: receives JPEG frames from the server via WebSocket
 * and displays them on the AMOLED. All rendering happens server-side.
 * The ESP32 is just a wireless display.
 *
 * Protocol:
 *   - Binary WebSocket messages = JPEG frame data → decode and display
 *   - Text messages = JSON commands (brightness, config, etc.)
 *   - ESP32 sends touch events + ping back to server
 *
 * Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75
 * Display: 466x466 AMOLED with CO5300 driver (QSPI)
 *
 * Required Libraries:
 *   - Arduino_GFX_Library
 *   - ArduinoWebsockets
 *   - ArduinoJson
 *   - JPEGDEC (by Larry Bank) — fast JPEG decoder
 *
 * Author: Ada & Chase
 * License: MIT
 */

#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <JPEGDEC.h>
#include "config.h"

using namespace websockets;

// ============================================
// Display Setup (Waveshare CO5300 QSPI)
// ============================================
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3
);

Arduino_GFX *gfx = new Arduino_CO5300(bus, TFT_RST, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 6, 0, 0, 0);

// ============================================
// JPEG Decoder
// ============================================
JPEGDEC jpeg;

// ============================================
// WebSocket Client
// ============================================
WebsocketsClient wsClient;
bool wsConnected = false;
unsigned long lastReconnectAttempt = 0;

// ============================================
// Frame Buffer (PSRAM) for incoming JPEG data
// ============================================
uint8_t *jpegBuffer = NULL;
size_t jpegBufferSize = 0;
volatile bool frameReady = false;

// ============================================
// Stats
// ============================================
unsigned long frameCount = 0;
unsigned long lastFpsReport = 0;
unsigned long lastFrameTime = 0;

// ============================================
// JPEG Draw Callback — writes decoded pixels directly to display
// ============================================
int jpegDrawCallback(JPEGDRAW *pDraw) {
    // pDraw contains decoded pixel block in RGB565 format
    // Write directly to display using batch operation
    gfx->draw16bitRGBBitmap(
        pDraw->x, pDraw->y,
        (uint16_t *)pDraw->pPixels,
        pDraw->iWidth, pDraw->iHeight
    );
    return 1; // Continue decoding
}

// ============================================
// Display a JPEG frame
// ============================================
void displayJpegFrame(uint8_t *data, size_t length) {
    if (!data || length == 0) return;

    gfx->beginWrite();

    if (jpeg.openRAM(data, length, jpegDrawCallback)) {
        // Decode with pixel scaling if JPEG is smaller than screen
        int imgW = jpeg.getWidth();
        int imgH = jpeg.getHeight();

        if (imgW == SCREEN_WIDTH && imgH == SCREEN_HEIGHT) {
            // Full resolution — decode directly
            jpeg.decode(0, 0, 0);
        } else {
            // Smaller image — center it on black background
            // First clear to black
            gfx->fillScreen(0x0000);

            // Calculate offset to center
            int offsetX = (SCREEN_WIDTH - imgW) / 2;
            int offsetY = (SCREEN_HEIGHT - imgH) / 2;

            // Decode at offset position
            jpeg.decode(offsetX, offsetY, 0);
        }

        jpeg.close();
    } else {
        Serial.printf("JPEG decode failed (error %d, %d bytes)\n",
                       jpeg.getLastError(), (int)length);
    }

    gfx->endWrite();
}

// ============================================
// WebSocket Callbacks
// ============================================
void onWebSocketMessage(WebsocketsMessage message) {
    if (message.isBinary()) {
        // Binary message = JPEG frame
        size_t len = message.length();
        const char *data = message.c_str();

        if (len > 0 && len < JPEG_BUFFER_SIZE) {
            memcpy(jpegBuffer, data, len);
            jpegBufferSize = len;
            frameReady = true;
        }
    } else {
        // Text message = JSON command
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, message.data());

        if (!error) {
            String msgType = doc["type"].as<String>();

            if (msgType == "config") {
                // Display configuration
                if (doc.containsKey("brightness")) {
                    int brightness = doc["brightness"].as<int>();
                    ((Arduino_CO5300*)gfx)->setBrightness(constrain(brightness, 0, 255));
                    Serial.printf("Brightness: %d\n", brightness);
                }
            } else if (msgType == "pong") {
                // Heartbeat response
            } else if (msgType == "clear") {
                gfx->fillScreen(0x0000);
            }
        }
    }
}

void onWebSocketEvent(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("WebSocket connected!");
        wsConnected = true;

        // Tell server we're a streaming display
        wsClient.send("{\"type\":\"hello\",\"mode\":\"stream\",\"width\":466,\"height\":466}");
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("WebSocket disconnected");
        wsConnected = false;
    }
}

void connectWebSocket() {
    String url = "ws://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + SERVER_PATH;
    Serial.print("Connecting to: ");
    Serial.println(url);

    wsClient.onMessage(onWebSocketMessage);
    wsClient.onEvent(onWebSocketEvent);
    wsClient.connect(url);
}

// ============================================
// Setup
// ============================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000) delay(10);
    delay(500);
    Serial.println("\n=== Ada Display Starting ===");

    // Initialize I2C
    Wire.begin(IIC_SDA, IIC_SCL);

    // Initialize display
    if (!gfx->begin()) {
        Serial.println("ERROR: Display init failed!");
        while (1) delay(100);
    }

    gfx->fillScreen(0x0000);
    ((Arduino_CO5300*)gfx)->setBrightness(DISPLAY_BRIGHTNESS);

    // Quick display test
    gfx->fillScreen(0xFFFF); delay(300);
    gfx->fillScreen(0x0000); delay(100);

    Serial.println("Display ready (466x466 AMOLED)");

    // Allocate JPEG buffer in PSRAM
    jpegBuffer = (uint8_t *)ps_malloc(JPEG_BUFFER_SIZE);
    if (!jpegBuffer) {
        Serial.println("ERROR: Failed to allocate JPEG buffer!");
        // Try regular malloc as fallback
        jpegBuffer = (uint8_t *)malloc(JPEG_BUFFER_SIZE);
    }

    if (jpegBuffer) {
        Serial.printf("JPEG buffer: %d KB\n", JPEG_BUFFER_SIZE / 1024);
    } else {
        Serial.println("FATAL: No memory for JPEG buffer");
        while (1) delay(100);
    }

    // Show startup message
    gfx->setTextColor(0x07FF);
    gfx->setTextSize(2);
    gfx->setCursor(SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT / 2 - 20);
    gfx->println("Ada");
    gfx->setCursor(SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 + 10);
    gfx->setTextSize(1);
    gfx->println("Connecting...");

    // Connect to WiFi
    Serial.printf("WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi OK! IP: %s\n", WiFi.localIP().toString().c_str());
        connectWebSocket();
    } else {
        Serial.println("\nWiFi failed — no server connection");
    }

    lastFpsReport = millis();
    lastFrameTime = millis();

    Serial.println("=== Ada Display Ready ===");
    Serial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Heap free: %d bytes\n", ESP.getFreeHeap());
}

// ============================================
// Main Loop
// ============================================
void loop() {
    unsigned long now = millis();

    // Poll WebSocket (always — no frame rate gating for receiving)
    if (wsConnected) {
        wsClient.poll();
    } else if (now - lastReconnectAttempt > WS_RECONNECT_INTERVAL_MS) {
        lastReconnectAttempt = now;
        if (WiFi.status() == WL_CONNECTED) {
            connectWebSocket();
        } else {
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    }

    // Display frame if one is ready
    if (frameReady) {
        frameReady = false;
        displayJpegFrame(jpegBuffer, jpegBufferSize);
        frameCount++;
        lastFrameTime = now;
    }

    // Periodic ping to keep connection alive
    if (wsConnected && now - lastFrameTime > 5000) {
        wsClient.send("{\"type\":\"ping\"}");
    }

    // FPS report every 10 seconds
    if (now - lastFpsReport >= 10000) {
        float fps = frameCount * 1000.0f / (now - lastFpsReport + 1);
        Serial.printf("FPS: %.1f | PSRAM: %d | Heap: %d\n",
                       fps, ESP.getFreePsram(), ESP.getFreeHeap());
        frameCount = 0;
        lastFpsReport = now;
    }
}
