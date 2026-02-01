/**
 * Ada Particles — ESP32 Belly Display Firmware
 *
 * Renders images as living particle clouds on a 466x466 AMOLED display.
 * Receives image data and particle physics config from Ada's server via WebSocket.
 * Particles float, swirl, pulse, and drift based on Ada's emotional state.
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
#include <ESP32_IO_Expander.h>
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

Arduino_GFX *gfx = new Arduino_CO5300(bus, TFT_RST, 0, false, SCREEN_WIDTH, SCREEN_HEIGHT);

// I/O Expander for power management
ESP32_IO_Expander *expander = NULL;

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
uint8_t *imageBuffer = NULL;   // Decoded RGB pixels
char *jsonBuffer = NULL;       // Incoming JSON string

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
    // Store message for processing in main loop (avoid blocking WS callback)
    pendingMessage = message.data();
    messageReady = true;
}

void onWebSocketEvent(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        USBSerial.println("WebSocket connected!");
        wsConnected = true;
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        USBSerial.println("WebSocket disconnected");
        wsConnected = false;
    }
}

void connectWebSocket() {
    String url = "ws://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + SERVER_PATH;
    USBSerial.print("Connecting to: ");
    USBSerial.println(url);

    wsClient.onMessage(onWebSocketMessage);
    wsClient.onEvent(onWebSocketEvent);
    wsClient.connect(url);
}

// ============================================
// Message Processing
// ============================================
void processMessage(const String& message) {
    // Parse JSON — use PSRAM for the document
    DynamicJsonDocument* doc = new (ps_malloc(sizeof(DynamicJsonDocument)))
        DynamicJsonDocument(JSON_DOC_SIZE);

    if (!doc) {
        USBSerial.println("ERROR: Failed to allocate JSON document");
        return;
    }

    DeserializationError error = deserializeJson(*doc, message);

    if (error) {
        USBSerial.print("JSON parse error: ");
        USBSerial.println(error.c_str());
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
        // Heartbeat response, ignore
    } else {
        USBSerial.print("Unknown message type: ");
        USBSerial.println(msgType);
    }

    doc->~DynamicJsonDocument();
    free(doc);
}

void handleParticlesMessage(JsonObject& root) {
    // Extract image data
    const char* imageB64 = root["image"];
    int imgWidth = root["width"] | 128;
    int imgHeight = root["height"] | 128;

    if (!imageB64) {
        USBSerial.println("No image data in particles message");
        return;
    }

    USBSerial.printf("Received particles: %dx%d image\n", imgWidth, imgHeight);

    // Decode base64 image
    size_t decodedLen = 0;
    bool decoded = base64_decode(imageB64, imageBuffer, &decodedLen);

    if (!decoded) {
        USBSerial.println("Base64 decode failed");
        return;
    }

    size_t expectedLen = imgWidth * imgHeight * 3;
    if (decodedLen < expectedLen) {
        USBSerial.printf("WARNING: Decoded %d bytes, expected %d\n",
                         (int)decodedLen, (int)expectedLen);
        // Pad with zeros if short
        if (decodedLen < expectedLen) {
            memset(imageBuffer + decodedLen, 0, expectedLen - decodedLen);
        }
    }

    // Parse config
    if (root.containsKey("config")) {
        JsonObject cfg = root["config"];
        particleSystem.parseConfig(cfg);
    }

    // Create particles from image
    particleSystem.createFromImage(imageBuffer, imgWidth, imgHeight);
}

void handleMoodMessage(JsonObject& root) {
    if (root.containsKey("config")) {
        JsonObject cfg = root["config"];
        particleSystem.parseConfig(cfg);
        USBSerial.println("Mood update received");
    }
}

void handleClearMessage() {
    particleSystem.clear();
    USBSerial.println("Clear received");
}

// ============================================
// Setup
// ============================================
void setup() {
    USBSerial.begin(115200);
    delay(1000);
    USBSerial.println("\n\n=== Ada Particles Starting ===");

    // Initialize I2C
    Wire.begin(IIC_SDA, IIC_SCL);

    // Initialize I/O expander for power
    expander = new ESP32_IO_Expander_TCA95xx_8bit(0x20);
    expander->init();
    expander->pinMode(0xFF, OUTPUT);
    expander->digitalWrite(0, HIGH);   // Enable power
    expander->digitalWrite(2, HIGH);   // Display power

    // Initialize display
    if (!gfx->begin()) {
        USBSerial.println("Display init failed!");
        while (1) delay(100);
    }

    gfx->fillScreen(0x0000);
    gfx->Display_Brightness(DISPLAY_BRIGHTNESS);

    USBSerial.println("Display initialized (466x466 AMOLED)");

    // Allocate PSRAM buffers
    imageBuffer = (uint8_t*)ps_malloc(MAX_IMAGE_BYTES);
    if (!imageBuffer) {
        USBSerial.println("ERROR: Failed to allocate image buffer in PSRAM");
    } else {
        USBSerial.printf("Image buffer: %d bytes in PSRAM\n", MAX_IMAGE_BYTES);
    }

    // Initialize particle system
    if (!particleSystem.init()) {
        USBSerial.println("ERROR: Particle system init failed");
        while (1) delay(100);
    }

    // Show startup text
    gfx->setTextColor(0x07FF);  // Cyan
    gfx->setTextSize(2);
    gfx->setCursor(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 10);
    gfx->println("Ada Particles");
    delay(500);

    // Start startup animation
    particleSystem.startStartup();

    // Connect to WiFi
    USBSerial.print("Connecting to WiFi: ");
    USBSerial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
        delay(500);
        USBSerial.print(".");
        wifiAttempts++;

        // Keep rendering during WiFi connect
        float dt = 0.033f;
        particleSystem.update(dt);
        particleSystem.render(gfx);
    }

    if (WiFi.status() == WL_CONNECTED) {
        USBSerial.println("\nWiFi connected!");
        USBSerial.print("IP: ");
        USBSerial.println(WiFi.localIP());

        connectWebSocket();
    } else {
        USBSerial.println("\nWiFi failed — running offline");
    }

    lastFrameTime = millis();
    lastFpsReport = millis();

    USBSerial.println("=== Ada Particles Ready ===");
    USBSerial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());
    USBSerial.printf("Heap free: %d bytes\n", ESP.getFreeHeap());
}

// ============================================
// Main Loop
// ============================================
void loop() {
    unsigned long currentTime = millis();
    float dt = (currentTime - lastFrameTime) / 1000.0f;

    // Cap dt to prevent physics explosions after lag spikes
    if (dt > 0.1f) dt = 0.1f;

    // Target frame rate
    if (dt < FRAME_TIME_MS / 1000.0f) {
        // Still poll WebSocket during wait
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
            // Try reconnecting WiFi
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    }

    // Process pending message (if any)
    if (messageReady) {
        messageReady = false;
        processMessage(pendingMessage);
        pendingMessage = "";
    }

    // Update particle physics
    particleSystem.update(dt);

    // Render frame
    particleSystem.render(gfx);

    // Send periodic ping to keep connection alive
    if (wsConnected && frameCount % (TARGET_FPS * 10) == 0) {
        wsClient.send("{\"type\":\"ping\"}");
    }

    // FPS reporting (every 10 seconds)
    if (currentTime - lastFpsReport >= 10000) {
        float fps = frameCount * 1000.0f / (currentTime - lastFpsReport + 1);
        USBSerial.printf("FPS: %.1f | Particles: %d | PSRAM: %d | Heap: %d\n",
                         fps, particleSystem.getActiveCount(),
                         ESP.getFreePsram(), ESP.getFreeHeap());
        frameCount = 0;
        lastFpsReport = currentTime;
    }
}
