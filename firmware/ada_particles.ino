/**
 * Ada Particles — ESP32 Belly Display Firmware
 *
 * Renders images as living particle clouds on a 466x466 AMOLED display.
 * Receives image data and particle physics config from Ada's server via WebSocket.
 * Particles float, swirl, pulse, and drift based on Ada's emotional state.
 *
 * Uses PSRAM-backed Canvas framebuffer for tear-free rendering and proper
 * circle/shape rendering (QSPI displays need bulk transfers).
 *
 * Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75
 * Display: 466x466 AMOLED with CO5300 driver (QSPI)
 *
 * Author: Ada & Chase
 * License: MIT
 */

#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
// ESP32_IO_Expander is optional - uncomment if you have the library installed
// #define USE_IO_EXPANDER
#ifdef USE_IO_EXPANDER
#include <ESP32_IO_Expander.h>
#endif
#include "config.h"
#include "base64_decode.h"
#include "particle_system.h"

using namespace websockets;

// ============================================
// Display Setup (Waveshare CO5300 QSPI)
// ============================================
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS,   // CS
    TFT_SCK,  // SCK
    TFT_D0,   // D0
    TFT_D1,   // D1
    TFT_D2,   // D2
    TFT_D3    // D3
);

Arduino_GFX *gfx = new Arduino_CO5300(bus, TFT_RST, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 6, 0, 0, 0);

// ============================================
// PSRAM-backed Canvas for tear-free rendering
// ============================================
Arduino_Canvas *canvas = nullptr;

// I/O Expander for power management (optional)
#ifdef USE_IO_EXPANDER
ESP32_IO_Expander *expander = NULL;
#endif

// ============================================
// WebSocket Client
// ============================================
WebsocketsClient wsClient;
bool wsConnected = false;
unsigned long lastReconnectAttempt = 0;

// ============================================
// Particle System
// ============================================
ParticleSystem particleSystem;

// ============================================
// Image Buffer (PSRAM)
// ============================================
uint8_t *imageBuffer = NULL;

// ============================================
// Timing
// ============================================
unsigned long lastFrameTime = 0;
unsigned long frameCount = 0;
unsigned long lastFpsReport = 0;

// ============================================
// Message Processing Flag
// ============================================
volatile bool messageReady = false;
String pendingMessage = "";

// ============================================
// Forward Declarations
// ============================================
void onWebSocketMessage(WebsocketsMessage message);
void onWebSocketEvent(WebsocketsEvent event, String data);
void connectWebSocket();
void processMessage(const String& message);
void handleParticlesMessage(JsonObject& root);
void handleMoodMessage(JsonObject& root);
void handleClearMessage();

// ============================================
// WebSocket Callbacks
// ============================================
void onWebSocketMessage(WebsocketsMessage message) {
    pendingMessage = message.data();
    messageReady = true;
}

void onWebSocketEvent(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("WebSocket connected!");
        wsConnected = true;
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
// Message Processing
// ============================================
void processMessage(const String& message) {
    DynamicJsonDocument* doc = new (ps_malloc(sizeof(DynamicJsonDocument)))
        DynamicJsonDocument(JSON_DOC_SIZE);

    if (!doc) {
        Serial.println("ERROR: Failed to allocate JSON document");
        return;
    }

    DeserializationError error = deserializeJson(*doc, message);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        doc->~DynamicJsonDocument();
        free(doc);
        return;
    }

    String msgType = (*doc)["type"].as<String>();

    if (msgType == "particles") {
        JsonObject root = doc->as<JsonObject>();
        handleParticlesMessage(root);
    } else if (msgType == "mood") {
        JsonObject root = doc->as<JsonObject>();
        handleMoodMessage(root);
    } else if (msgType == "clear") {
        handleClearMessage();
    } else if (msgType == "pong") {
        // Heartbeat response
    } else {
        Serial.print("Unknown message type: ");
        Serial.println(msgType);
    }

    doc->~DynamicJsonDocument();
    free(doc);
}

void handleParticlesMessage(JsonObject& root) {
    const char* imageB64 = root["image"];
    int imgWidth = root["width"] | 64;
    int imgHeight = root["height"] | 64;

    if (!imageB64) {
        Serial.println("No image data in particles message");
        return;
    }

    Serial.printf("Received particles: %dx%d image\n", imgWidth, imgHeight);

    size_t decodedLen = 0;
    bool decoded = base64_decode(imageB64, imageBuffer, &decodedLen);

    if (!decoded) {
        Serial.println("Base64 decode failed");
        return;
    }

    size_t expectedLen = imgWidth * imgHeight * 3;
    if (decodedLen < expectedLen) {
        Serial.printf("WARNING: Decoded %d bytes, expected %d\n",
                         (int)decodedLen, (int)expectedLen);
        if (decodedLen < expectedLen) {
            memset(imageBuffer + decodedLen, 0, expectedLen - decodedLen);
        }
    }

    if (root.containsKey("config")) {
        JsonObject cfg = root["config"];
        particleSystem.parseConfig(cfg);
    }

    particleSystem.createFromImage(imageBuffer, imgWidth, imgHeight);
}

void handleMoodMessage(JsonObject& root) {
    if (root.containsKey("config")) {
        JsonObject cfg = root["config"];
        particleSystem.parseConfig(cfg);
        Serial.println("Mood update received");
    }
}

void handleClearMessage() {
    particleSystem.clear();
    Serial.println("Clear received");
}

// ============================================
// Setup
// ============================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000) {
        delay(10);
    }
    delay(1000);
    Serial.println("\n\n=== Ada Particles Starting ===");
    Serial.flush();

    // Initialize I2C
    Wire.begin(IIC_SDA, IIC_SCL);

#ifdef USE_IO_EXPANDER
    expander = new ESP32_IO_Expander_TCA95xx_8bit(0x20);
    expander->init();
    expander->pinMode(0xFF, OUTPUT);
    expander->digitalWrite(0, HIGH);
    expander->digitalWrite(2, HIGH);
    Serial.println("I/O Expander initialized");
#else
    Serial.println("I/O Expander disabled");
#endif

    // Initialize display
    Serial.println("Initializing display...");
    if (!gfx->begin()) {
        Serial.println("ERROR: Display init failed!");
        while (1) delay(100);
    }

    Serial.println("Display begin() succeeded");
    gfx->fillScreen(0x0000);
    delay(100);

    // Set brightness
    Serial.printf("Setting brightness to %d\n", DISPLAY_BRIGHTNESS);
    ((Arduino_CO5300*)gfx)->setBrightness(DISPLAY_BRIGHTNESS);
    delay(100);

    // Quick display test
    Serial.println("Display test...");
    gfx->fillScreen(0xFFFF); delay(500);
    gfx->fillScreen(0xF800); delay(300);
    gfx->fillScreen(0x07E0); delay(300);
    gfx->fillScreen(0x001F); delay(300);
    gfx->fillScreen(0x0000); delay(200);

    // ============================================
    // Initialize Canvas (PSRAM-backed framebuffer)
    // This is the KEY to proper rendering:
    // - fillCircle works correctly (pixel-level math in RAM)
    // - No tearing (entire frame written to display at once)
    // - Faster than individual QSPI draw calls
    // Uses ~424KB of PSRAM (466 * 466 * 2 bytes)
    // ============================================
    Serial.println("Creating PSRAM canvas...");
    canvas = new Arduino_Canvas(SCREEN_WIDTH, SCREEN_HEIGHT, gfx);
    if (!canvas->begin()) {
        Serial.println("WARNING: Canvas init failed! Falling back to direct rendering.");
        // If canvas fails, we'll use gfx directly (with tearing)
        delete canvas;
        canvas = nullptr;
    } else {
        Serial.printf("Canvas ready: %dx%d (%d KB PSRAM)\n",
                       SCREEN_WIDTH, SCREEN_HEIGHT,
                       (SCREEN_WIDTH * SCREEN_HEIGHT * 2) / 1024);
    }

    Serial.println("Display initialized (466x466 AMOLED)");

    // Allocate PSRAM buffers for image data
    imageBuffer = (uint8_t*)ps_malloc(MAX_IMAGE_BYTES);
    if (!imageBuffer) {
        Serial.println("ERROR: Failed to allocate image buffer in PSRAM");
    } else {
        Serial.printf("Image buffer: %d bytes in PSRAM\n", MAX_IMAGE_BYTES);
    }

    // Initialize particle system
    if (!particleSystem.init()) {
        Serial.println("ERROR: Particle system init failed");
        while (1) delay(100);
    }

    // Show startup text on the actual display (not canvas)
    gfx->setTextColor(0x07FF);
    gfx->setTextSize(2);
    gfx->setCursor(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 10);
    gfx->println("Ada Particles");
    delay(500);

    // Start startup animation
    particleSystem.startStartup();

    // Connect to WiFi
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Get the render target (canvas if available, otherwise gfx)
    Arduino_GFX *renderTarget = canvas ? (Arduino_GFX*)canvas : gfx;

    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
        delay(500);
        Serial.print(".");
        wifiAttempts++;

        // Keep rendering during WiFi connect
        float dt = 0.033f;
        particleSystem.update(dt);
        particleSystem.render(renderTarget);
        if (canvas) canvas->flush();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        connectWebSocket();
    } else {
        Serial.println("\nWiFi failed — running offline");
    }

    lastFrameTime = millis();
    lastFpsReport = millis();

    Serial.println("=== Ada Particles Ready ===");
    Serial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Heap free: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Canvas: %s\n", canvas ? "ENABLED (tear-free)" : "DISABLED (direct)");
}

// ============================================
// Main Loop
// ============================================
void loop() {
    unsigned long currentTime = millis();
    float dt = (currentTime - lastFrameTime) / 1000.0f;

    if (dt > 0.1f) dt = 0.1f;

    // Target frame rate
    if (dt < FRAME_TIME_MS / 1000.0f) {
        if (wsConnected) {
            wsClient.poll();
        }
        return;
    }

    lastFrameTime = currentTime;
    frameCount++;

    // Handle WebSocket
    if (wsConnected) {
        wsClient.poll();
    } else if (currentTime - lastReconnectAttempt > WS_RECONNECT_INTERVAL_MS) {
        lastReconnectAttempt = currentTime;
        if (WiFi.status() == WL_CONNECTED) {
            connectWebSocket();
        } else {
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    }

    // Process pending message
    if (messageReady) {
        messageReady = false;
        processMessage(pendingMessage);
        pendingMessage = "";
    }

    // Update particle physics
    particleSystem.update(dt);

    // ============================================
    // Render to canvas (framebuffer), then flush
    // This eliminates tearing and fixes circle rendering
    // ============================================
    Arduino_GFX *renderTarget = canvas ? (Arduino_GFX*)canvas : gfx;
    particleSystem.render(renderTarget);
    if (canvas) {
        canvas->flush();  // Blast entire framebuffer to display
    }

    // Periodic ping
    if (wsConnected && frameCount % (TARGET_FPS * 10) == 0) {
        wsClient.send("{\"type\":\"ping\"}");
    }

    // FPS reporting
    if (currentTime - lastFpsReport >= 10000) {
        float fps = frameCount * 1000.0f / (currentTime - lastFpsReport + 1);
        Serial.printf("FPS: %.1f | Particles: %d | Canvas: %s | PSRAM: %d | Heap: %d\n",
                         fps, particleSystem.getActiveCount(),
                         canvas ? "ON" : "OFF",
                         ESP.getFreePsram(), ESP.getFreeHeap());
        frameCount = 0;
        lastFpsReport = currentTime;
    }
}
