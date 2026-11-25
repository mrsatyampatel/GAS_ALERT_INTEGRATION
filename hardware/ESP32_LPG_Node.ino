#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ================= 1. HARDWARE PIN CONFIGURATION =================
#define MQ6_PIN 34
#define BUZZER_PIN 25
int threshold = 1800;
// =================================================================

// ================= 2. WI-FI & RENDER CLOUD CONFIGURATION =========
// Apna Wi-Fi / Hotspot details daalein:
const char* ssid = "system";            // <-- Apna WiFi naam
const char* password = "AAPKA_PASSWORD"; // <-- Apna WiFi Password

// Aapka Live Render Public Cloud URLs (24/7 Active Worldwide):
String emergencyUrl = "https://lpgenius-9gls.onrender.com/api/v1/alerts/gas-leak";
String liveTelemetryUrl = "https://lpgenius-9gls.onrender.com/dashboard/api/live";

// Backend Secret Key
const char* deviceSecret = "esp32_super_secret_key_123";
// =================================================================

// Timers
unsigned long lastAlertTime = 0;
bool firstAlertSent = false; // Boot hone ke turant baad pehla alert bina 30s wait kiye bhejne ke liye
const unsigned long ALERT_INTERVAL = 30000; // Emergency alert har 30s me ek baar dispatch hoga

unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 3000; // Live reading har 3 second me website ko jayegi

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  // Wi-Fi Connection Setup
  Serial.println("\n======================================");
  Serial.println("  LPG Guard System (Live Cloud Node)  ");
  Serial.println("======================================");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // ESP32 background me khud reconnect sambhalega bina spam kiye
  WiFi.persistent(true);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connected Successfully!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Target Server: https://lpgenius-9gls.onrender.com");
}

void loop() {
  // Wi-Fi Reconnect (Har 10 second me check karega, taaki loop spam na ho)
  static unsigned long lastWiFiCheck = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    Serial.println("⚠️ WiFi Disconnected! Reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
  }

  // 1. Read Gas Sensor
  int gasValue = analogRead(MQ6_PIN);
  
  Serial.print("MQ-6 Reading: ");
  Serial.print(gasValue);
  Serial.println(" PPM");

  // 2. DANGER ZONE CONDITION (gas > 1800)
  if (gasValue > threshold) {
    tone(BUZZER_PIN, 2500);  // 🔊 Hardware Buzzer ON

    // Pehla alert turant bheje (boot ke 30s ke andar bhi!), uske baad 30s cooldown
    if (!firstAlertSent || (millis() - lastAlertTime > ALERT_INTERVAL)) {
      Serial.println("🚨 [EMERGENCY] Leak Detected! Dispatching to Render Cloud...");
      if (sendEmergencyAlert(gasValue)) {
        firstAlertSent = true;
        lastAlertTime = millis();
      } else {
        Serial.println("⚠️ Alert send failed, will retry!");
      }
    }
  } 
  else {
    noTone(BUZZER_PIN);      // 🔇 Hardware Buzzer OFF (Safe)
  }

  // 3. CONTINUOUS LIVE TELEMETRY (Har 2 second me website dashboard par live needle update)
  if (millis() - lastTelemetryTime > TELEMETRY_INTERVAL) {
    sendLiveTelemetry(gasValue);
    lastTelemetryTime = millis();
  }

  delay(300);
}

// -------------------------------------------------------------
// Function 1: Emergency Alert Dispatch (SMS + WhatsApp + Pushover)
// -------------------------------------------------------------
bool sendEmergencyAlert(int gasLevel) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Cannot send alert: WiFi disconnected!");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Render HTTPS SSL connection bina kisi error ke allow kare

  HTTPClient http;
  http.setTimeout(10000); // 10s timeout for cloud HTTPS
  http.begin(client, emergencyUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-secret", deviceSecret);

  String payload = "{"
    "\"deviceId\":\"ESP32_NODE_01\","
    "\"location\":\"Kitchen Cylinder Area\","
    "\"gasLevel\":" + String(gasLevel) + ","
    "\"threshold\":" + String(threshold) + ","
    "\"severity\":\"CRITICAL\""
  "}";

  int code = http.POST(payload);
  Serial.print("🚨 Cloud Emergency Alert Sent -> Response Code: ");
  Serial.println(code);
  if (code > 0) {
    Serial.println(http.getString());
  } else {
    Serial.print("❌ HTTP Error: ");
    Serial.println(http.errorToString(code));
  }
  http.end();
  return (code >= 200 && code < 300);
}

// -------------------------------------------------------------
// Function 2: Real-Time Live Telemetry (Website Speedometer Needle)
// -------------------------------------------------------------
void sendLiveTelemetry(int gasLevel) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Render HTTPS SSL connection bina kisi error ke allow kare

  HTTPClient http;
  http.begin(client, liveTelemetryUrl);
  http.addHeader("Content-Type", "application/json");

  String payload = "{"
    "\"deviceId\":\"ESP32_NODE_01\","
    "\"location\":\"Kitchen Cylinder Area\","
    "\"gasLevel\":" + String(gasLevel) + ","
    "\"threshold\":" + String(threshold) +
  "}";

  int code = http.POST(payload);
  if (code > 0) {
    Serial.print("📡 Live Telemetry Synced with Cloud -> ");
    Serial.print(gasLevel);
    Serial.println(" PPM");
  } else {
    Serial.print("⚠️ Cloud Telemetry failed, Error code: ");
    Serial.println(code);
  }
  http.end();
}
