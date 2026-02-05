/**
 * Ada Particles — Dreamy Particle Display for ESP32
 * 
 * A fluid, organic particle system that gives Ada a living presence.
 * Particles drift like they're suspended in water, form shapes when
 * Ada wants to express something, and respond to mood through color.
 * 
 * Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75
 *   - 466x466 AMOLED display (CO5300 via QSPI)
 *   - ESP32-S3 with 8MB PSRAM
 *   - Capacitive touch (CST9217)
 *   - WiFi for server connection
 * 
 * Author: Ada & Chase
 * License: MIT
 */

#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

#include "config.h"
#include "src/particle_system.h"

using namespace websockets;

// ============================================
// Display Setup
// ============================================

Arduino_DataBus* bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3
);

Arduino_GFX* gfx = new Arduino_CO5300(
    bus, TFT_RST, DISPLAY_ROTATION, 
    false, SCREEN_WIDTH, SCREEN_HEIGHT
);

// ============================================
// WebSocket Client
// ============================================

WebsocketsClient wsClient;
bool wsConnected = false;
unsigned long lastReconnectAttempt = 0;
unsigned long lastPing = 0;

// ============================================
// Timing
// ============================================

unsigned long lastFrameTime = 0;
unsigned long lastStatusReport = 0;

// ============================================
// WebSocket Handlers
// ============================================

void onWebSocketMessage(WebsocketsMessage message) {
    if (message.isBinary()) {
        // Binary messages not used in native mode
        return;
    }
    
    // Parse JSON state message
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, message.data());
    
    if (error) {
        DEBUG_PRINTF("JSON parse error: %s\n", error.c_str());
        return;
    }
    
    String msgType = doc["type"] | "";
    
    if (msgType == "state") {
        // Mood update
        if (doc.containsKey("mood")) {
            float valence = doc["mood"]["valence"] | 0.0f;
            float arousal = doc["mood"]["arousal"] | 0.3f;
            particleSystem.setMood(valence, arousal);
        }
        
        // Formation update
        if (doc.containsKey("formation")) {
            String formation = doc["formation"] | "idle";
            uint16_t transitionMs = doc["transition_ms"] | DEFAULT_TRANSITION_MS;
            
            FormationType ft = FORMATION_IDLE;
            if (formation == "cloud") ft = FORMATION_CLOUD;
            else if (formation == "sun") ft = FORMATION_SUN;
            else if (formation == "rain") ft = FORMATION_RAIN;
            else if (formation == "snow") ft = FORMATION_SNOW;
            else if (formation == "heart") ft = FORMATION_HEART;
            else if (formation == "thinking") ft = FORMATION_THINKING;
            else if (formation == "wave") ft = FORMATION_WAVE;
            
            particleSystem.setFormation(ft, transitionMs);
        }
        
        // Particle count
        if (doc.containsKey("particle_count")) {
            int count = doc["particle_count"] | DEFAULT_PARTICLE_COUNT;
            particleSystem.setParticleCount(count);
        }
    } else if (msgType == "config") {
        // Display brightness
        if (doc.containsKey("brightness")) {
            int brightness = doc["brightness"] | DISPLAY_BRIGHTNESS;
            ((Arduino_CO5300*)gfx)->setBrightness(constrain(brightness, 0, 255));
        }
    } else if (msgType == "pong") {
        // Heartbeat response
    }
}

void onWebSocketEvent(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        DEBUG_PRINTLN("WebSocket connected!");
        wsConnected = true;
        particleSystem.setDisconnected(false);
        
        // Send hello
        wsClient.send("{\"type\":\"hello\",\"mode\":\"native\",\"width\":466,\"height\":466}");
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        DEBUG_PRINTLN("WebSocket disconnected");
        wsConnected = false;
        particleSystem.setDisconnected(true);
    }
}

void connectWebSocket() {
    String url = "ws://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + SERVER_PATH;
    DEBUG_PRINT("Connecting to: ");
    DEBUG_PRINTLN(url);
    
    wsClient.onMessage(onWebSocketMessage);
    wsClient.onEvent(onWebSocketEvent);
    wsClient.connect(url);
}

// ============================================
// Setup
// ============================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) delay(10);
    delay(100);
    
    Serial.println("\n========================================");
    Serial.println("   Ada Particles - Starting Up");
    Serial.println("========================================\n");
    
    // Initialize I2C
    Wire.begin(IIC_SDA, IIC_SCL);
    
    // Initialize display
    Serial.println("Initializing display...");
    if (!gfx->begin()) {
        Serial.println("ERROR: Display init failed!");
        while (1) { delay(100); }
    }
    
    gfx->fillScreen(0x0000);
    ((Arduino_CO5300*)gfx)->setBrightness(DISPLAY_BRIGHTNESS);
    Serial.println("Display ready");
    
    // Show startup message
    gfx->setTextColor(0x07FF);  // Cyan
    gfx->setTextSize(3);
    gfx->setCursor(SCREEN_CENTER_X - 50, SCREEN_CENTER_Y - 30);
    gfx->println("Ada");
    gfx->setTextSize(1);
    gfx->setCursor(SCREEN_CENTER_X - 60, SCREEN_CENTER_Y + 20);
    gfx->println("Initializing...");
    
    // Initialize particle system
    Serial.println("Initializing particle system...");
    if (!particleSystem.init(gfx)) {
        Serial.println("ERROR: Particle system init failed!");
        gfx->fillScreen(0xF800);  // Red = error
        while (1) { delay(100); }
    }
    
    // Connect to WiFi
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    gfx->setCursor(SCREEN_CENTER_X - 70, SCREEN_CENTER_Y + 40);
    gfx->println("Connecting WiFi...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
        connectWebSocket();
    } else {
        Serial.println("\nWiFi failed - running offline");
        particleSystem.setDisconnected(true);
    }
    
    // Clear startup text and start rendering
    gfx->fillScreen(0x0000);
    
    lastFrameTime = millis();
    lastStatusReport = millis();
    
    Serial.println("\n========================================");
    Serial.println("   Ada Particles - Ready!");
    Serial.printf("   PSRAM: %d KB free\n", ESP.getFreePsram() / 1024);
    Serial.printf("   Heap: %d KB free\n", ESP.getFreeHeap() / 1024);
    Serial.println("========================================\n");
}

// ============================================
// Main Loop
// ============================================

void loop() {
    unsigned long now = millis();
    
    // Calculate delta time
    float dt = (now - lastFrameTime) / 1000.0f;
    lastFrameTime = now;
    
    // Clamp dt to prevent huge jumps
    if (dt > 0.1f) dt = 0.1f;
    
    // Poll WebSocket
    if (wsConnected) {
        wsClient.poll();
        
        // Periodic ping
        if (now - lastPing > 10000) {
            wsClient.send("{\"type\":\"ping\"}");
            lastPing = now;
        }
    } else {
        // Try to reconnect
        if (now - lastReconnectAttempt > WS_RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = now;
            if (WiFi.status() == WL_CONNECTED) {
                connectWebSocket();
            } else {
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            }
        }
    }
    
    // Update particle system
    particleSystem.update(dt);
    
    // Render
    particleSystem.render();
    
    // Status report
    #ifdef DEBUG_ENABLED
    if (now - lastStatusReport >= FPS_REPORT_INTERVAL_MS) {
        lastStatusReport = now;
        Serial.printf("FPS: %.1f | Particles: %d | PSRAM: %d | Heap: %d | %s\n",
            particleSystem.getFPS(),
            particleSystem.getActiveParticles(),
            ESP.getFreePsram(),
            ESP.getFreeHeap(),
            wsConnected ? "Connected" : "Disconnected"
        );
    }
    #endif
    
    // Frame rate limiting
    unsigned long frameTime = millis() - now;
    if (frameTime < TARGET_FRAME_TIME_MS) {
        delay(TARGET_FRAME_TIME_MS - frameTime);
    }
}
