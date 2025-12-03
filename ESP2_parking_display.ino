#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "parking";
const char* password = "0000011111";

// Server IP (will be updated with actual laptop IP)
const char* serverIP = "10.137.170.161";    // Your laptop IP

// Pin definitions for ESP32 (boot-safe pins)
#define SLOT1_IR_PIN    32  // GPIO32
#define SLOT2_IR_PIN    33  // GPIO33
#define SLOT3_IR_PIN    25  // GPIO25
#define SLOT4_IR_PIN    26  // GPIO26

// Web server for receiving commands from Flask server
WebServer server(80);

// State variables
String lastPlate = "";
String gateStatus = "Gate Closed";
String lastMessage = "System Ready";
bool messageIsError = false;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== ESP32 Parking Display Controller ===");
  Serial.println("IR Sensor Pins:");
  Serial.println("Slot 1: GPIO32");
  Serial.println("Slot 2: GPIO33");
  Serial.println("Slot 3: GPIO25");
  Serial.println("Slot 4: GPIO26");
  Serial.println("Web Interface: http://[ESP_IP]/");
  Serial.println("================================\n");
  
  // Initialize pins
  pinMode(SLOT1_IR_PIN, INPUT_PULLUP);
  pinMode(SLOT2_IR_PIN, INPUT_PULLUP);
  pinMode(SLOT3_IR_PIN, INPUT_PULLUP);
  pinMode(SLOT4_IR_PIN, INPUT_PULLUP);
  
  // Initialize web interface
  lastMessage = "System Ready";
  messageIsError = false;
  
  // Use DHCP to get IP on same subnet
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
  
  // Setup web server endpoints
  server.on("/", handleRoot);
  server.on("/display_bill", HTTP_POST, handleDisplayBill);
  server.on("/set_plate", HTTP_POST, handleSetPlate);
  server.on("/api/status", handleAPIStatus);
  server.on("/status", []() {
    server.send(200, "text/plain", "Parking/Display Controller Ready (ESP32)");
  });
  server.begin();
  
  Serial.println("ESP32 Parking/Display Controller Ready");
  Serial.println("Access web interface at: http://" + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
  
  // Debug: Print raw sensor values every 5 seconds
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    lastDebug = millis();
    int raw1 = digitalRead(SLOT1_IR_PIN);
    int raw2 = digitalRead(SLOT2_IR_PIN);
    int raw3 = digitalRead(SLOT3_IR_PIN);
    int raw4 = digitalRead(SLOT4_IR_PIN);
    Serial.println("RAW: S1=" + String(raw1) + " S2=" + String(raw2) + " S3=" + String(raw3) + " S4=" + String(raw4));
  }
  
  // Check parking slots
  checkParkingSlots();
  
  delay(100);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Parking Display</title>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;}";
  html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
  html += ".status{padding:15px;margin:10px 0;border-radius:5px;font-weight:bold;}";
  html += ".success{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}";
  html += ".error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}";
  html += ".info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb;}";
  html += ".slots{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;margin:20px 0;}";
  html += ".slot{padding:20px;text-align:center;border-radius:5px;font-size:18px;font-weight:bold;}";
  html += ".available{background:#d4edda;color:#155724;}";
  html += ".occupied{background:#f8d7da;color:#721c24;}";
  html += "h1{color:#333;text-align:center;}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>🅿️ Smart Parking System</h1>";
  
  // Gate Status
  String statusClass = gateStatus.indexOf("Opening") >= 0 ? "success" : "info";
  html += "<div class='status " + statusClass + "'>🚪 " + gateStatus + "</div>";
  
  // Last Message
  String msgClass = messageIsError ? "error" : "success";
  html += "<div class='status " + msgClass + "'>📢 " + lastMessage + "</div>";
  
  // Available Slots
  html += "<h2>Available Parking Slots</h2>";
  html += "<div class='slots'>";
  
  bool slot1 = !digitalRead(SLOT1_IR_PIN);
  bool slot2 = !digitalRead(SLOT2_IR_PIN);
  bool slot3 = !digitalRead(SLOT3_IR_PIN);
  bool slot4 = !digitalRead(SLOT4_IR_PIN);
  
  html += "<div class='slot " + String(slot1 ? "occupied" : "available") + "'>Slot 1<br>" + String(slot1 ? "🚗 OCCUPIED" : "✅ AVAILABLE") + "</div>";
  html += "<div class='slot " + String(slot2 ? "occupied" : "available") + "'>Slot 2<br>" + String(slot2 ? "🚗 OCCUPIED" : "✅ AVAILABLE") + "</div>";
  html += "<div class='slot " + String(slot3 ? "occupied" : "available") + "'>Slot 3<br>" + String(slot3 ? "🚗 OCCUPIED" : "✅ AVAILABLE") + "</div>";
  html += "<div class='slot " + String(slot4 ? "occupied" : "available") + "'>Slot 4<br>" + String(slot4 ? "🚗 OCCUPIED" : "✅ AVAILABLE") + "</div>";
  
  html += "</div>";
  html += "<p style='text-align:center;color:#666;margin-top:20px;'>Last updated: " + String(millis()/1000) + "s</p>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSetPlate() {
  String body = server.arg("plain");
  Serial.println("📝 Plate notification: " + body);
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    Serial.println("❌ JSON parse error");
    lastMessage = "Error: Invalid plate data received";
    messageIsError = true;
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  lastPlate = doc["plate"].as<String>();
  gateStatus = "Gate Opening - Plate Detected";
  lastMessage = "Vehicle " + lastPlate + " detected - Checking slots...";
  messageIsError = false;
  
  Serial.println("📝 Last plate set to: " + lastPlate);
  Serial.println("🔍 Checking slots immediately...");
  
  server.send(200, "text/plain", "Plate set");
  
  // Check slots immediately after plate is set
  delay(500);
  checkParkingSlotsImmediate();
}

void handleDisplayBill() {
  String body = server.arg("plain");
  Serial.println("💰 Bill request: " + body);
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    Serial.println("❌ JSON parse error");
    lastMessage = "Error: Invalid bill data received";
    messageIsError = true;
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  String plate = doc["plate"];
  int amount = doc["amount"];
  
  gateStatus = "Gate Opening - Exit Approved";
  lastMessage = "Bill: " + plate + " - Rs." + String(amount) + " - Thank you!";
  messageIsError = false;
  
  Serial.println("💰 Bill displayed: " + plate + " - Rs." + String(amount));
  
  server.send(200, "text/plain", "Bill displayed");
}

void checkParkingSlotsImmediate() {
  bool slot1 = !digitalRead(SLOT1_IR_PIN);
  bool slot2 = !digitalRead(SLOT2_IR_PIN);
  bool slot3 = !digitalRead(SLOT3_IR_PIN);
  bool slot4 = !digitalRead(SLOT4_IR_PIN);
  
  Serial.println("🔍 Slot status: 1=" + String(slot1) + " 2=" + String(slot2) + " 3=" + String(slot3) + " 4=" + String(slot4));
  
  if (lastPlate != "" && (slot1 || slot2 || slot3 || slot4)) {
    int occupiedSlot = 0;
    if (slot1) occupiedSlot = 1;
    else if (slot2) occupiedSlot = 2;
    else if (slot3) occupiedSlot = 3;
    else if (slot4) occupiedSlot = 4;
    
    if (occupiedSlot > 0) {
      Serial.println("✅ Car detected in slot " + String(occupiedSlot));
      lastMessage = "Success: " + lastPlate + " parked in Slot " + String(occupiedSlot);
      messageIsError = false;
      updateSlotStatus(lastPlate, occupiedSlot);
      lastPlate = "";
    }
  } else if (lastPlate != "") {
    lastMessage = "Waiting: " + lastPlate + " - Please park in available slot";
    messageIsError = false;
    Serial.println("⚠️ Plate set but no car detected in any slot yet");
  }
}

void checkParkingSlots() {
  static unsigned long lastSlotCheck = 0;
  if (millis() - lastSlotCheck < 3000) return;
  lastSlotCheck = millis();
  
  checkParkingSlotsImmediate();
}

void updateSlotStatus(String plate, int slot) {
  HTTPClient http;
  
  String url = "http://" + String(serverIP) + ":5000/api/update_slot";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"plate\":\"" + plate + "\",\"slot\":" + String(slot) + "}";
  
  int httpCode = http.POST(payload);
  if (httpCode > 0) {
    Serial.println("🅿️ Slot updated: " + plate + " in slot " + String(slot));
    gateStatus = "Gate Closed";
  } else {
    lastMessage = "Error: Failed to update slot status";
    messageIsError = true;
  }
  
  http.end();
}

void handleAPIStatus() {
  bool slot1 = !digitalRead(SLOT1_IR_PIN);
  bool slot2 = !digitalRead(SLOT2_IR_PIN);
  bool slot3 = !digitalRead(SLOT3_IR_PIN);
  bool slot4 = !digitalRead(SLOT4_IR_PIN);
  
  String json = "{";
  json += "\"gateStatus\":\"" + gateStatus + "\",";
  json += "\"lastMessage\":\"" + lastMessage + "\",";
  json += "\"messageIsError\":" + String(messageIsError ? "true" : "false") + ",";
  json += "\"slots\":{";
  json += "\"slot1\":" + String(slot1 ? "true" : "false") + ",";
  json += "\"slot2\":" + String(slot2 ? "true" : "false") + ",";
  json += "\"slot3\":" + String(slot3 ? "true" : "false") + ",";
  json += "\"slot4\":" + String(slot4 ? "true" : "false");
  json += "},";
  json += "\"uptime\":" + String(millis()/1000);
  json += "}";
  
  server.send(200, "application/json", json);
}