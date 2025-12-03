#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "parking";
const char* password = "0000011111";

// Server IP (will be updated with actual laptop IP)
const char* serverIP = "10.137.170.161";    // Flask server IP

// Pin definitions for ESP32 (boot-safe pins)
#define OLED_SDA        21  // GPIO21
#define OLED_SCL        22  // GPIO22
#define SLOT1_IR_PIN    32  // GPIO32
#define SLOT2_IR_PIN    33  // GPIO33
#define SLOT3_IR_PIN    25  // GPIO25
#define SLOT4_IR_PIN    26  // GPIO26

// OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Web server for receiving commands from Flask server
WebServer server(80);

// State variables
String lastPlate = "";

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== ESP32 Parking Slot Monitor ===");
  Serial.println("IR Sensor Pins:");
  Serial.println("Slot 1: GPIO32");
  Serial.println("Slot 2: GPIO33");
  Serial.println("Slot 3: GPIO25");
  Serial.println("Slot 4: GPIO26");
  Serial.println("================================\n");
  
  // Initialize pins
  pinMode(SLOT1_IR_PIN, INPUT_PULLUP);
  pinMode(SLOT2_IR_PIN, INPUT_PULLUP);
  pinMode(SLOT3_IR_PIN, INPUT_PULLUP);
  pinMode(SLOT4_IR_PIN, INPUT_PULLUP);
  
  // Initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Smart Parking");
  display.println("System Ready");
  display.display();
  
  // Configure static IP
  WiFi.config(IPAddress(10, 137, 170, 70), 
             IPAddress(10, 137, 170, 1), 
             IPAddress(255, 255, 255, 0));
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
  
  // Setup web server endpoints
  server.on("/display_bill", HTTP_POST, handleDisplayBill);
  server.on("/set_plate", HTTP_POST, handleSetPlate);
  server.on("/status", []() {
    server.send(200, "text/plain", "Parking/Display Controller Ready (ESP32)");
  });
  server.begin();
  
  Serial.println("ESP32 Parking/Display Controller Ready");
}

void loop() {
  server.handleClient();
  
  // Debug: Print raw sensor values every 2 seconds
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 2000) {
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

void handleSetPlate() {
  String body = server.arg("plain");
  Serial.println("[PLATE] Plate notification: " + body);
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    Serial.println("[ERROR] JSON parse error");
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  lastPlate = doc["plate"].as<String>();
  Serial.println("[PLATE] Last plate set to: " + lastPlate);
  Serial.println("[CHECK] Checking slots immediately...");
  
  server.send(200, "text/plain", "Plate set");
  
  // Check slots immediately after plate is set
  delay(500);
  checkParkingSlotsImmediate();
}

void handleDisplayBill() {
  String body = server.arg("plain");
  Serial.println("[BILL] Bill request: " + body);
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    Serial.println("[ERROR] JSON parse error");
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  String plate = doc["plate"];
  int amount = doc["amount"];
  
  displayBill(plate, amount);
  server.send(200, "text/plain", "Bill displayed");
}

void updateMainDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("Smart Parking");
  display.println("System Ready");
  display.println("");
  display.println("Available Slots:");
  
  // Show available slots
  bool slot1 = !digitalRead(SLOT1_IR_PIN);
  bool slot2 = !digitalRead(SLOT2_IR_PIN);
  bool slot3 = !digitalRead(SLOT3_IR_PIN);
  bool slot4 = !digitalRead(SLOT4_IR_PIN);
  
  display.print("1:");
  display.print(slot1 ? "X" : "O");
  display.print(" 2:");
  display.print(slot2 ? "X" : "O");
  display.print(" 3:");
  display.print(slot3 ? "X" : "O");
  display.print(" 4:");
  display.println(slot4 ? "X" : "O");
  
  display.display();
}

void displayBill(String plate, int amount) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("PARKING BILL");
  display.println("============");
  display.println("");
  display.setTextSize(1);
  display.println("Plate: " + plate);
  display.println("");
  display.setTextSize(2);
  display.println("Rs." + String(amount));
  display.setTextSize(1);
  display.println("");
  display.println("Thank you!");
  display.display();
  
  Serial.println("[BILL] Bill displayed: " + plate + " - Rs." + String(amount));
  
  // Auto clear after 10 seconds
  delay(10000);
  updateMainDisplay(); // Reset to main screen
}

void checkParkingSlotsImmediate() {
  bool slot1 = !digitalRead(SLOT1_IR_PIN);
  bool slot2 = !digitalRead(SLOT2_IR_PIN);
  bool slot3 = !digitalRead(SLOT3_IR_PIN);
  bool slot4 = !digitalRead(SLOT4_IR_PIN);
  
  Serial.println("[CHECK] Slot status: 1=" + String(slot1) + " 2=" + String(slot2) + " 3=" + String(slot3) + " 4=" + String(slot4));
  
  // Check if any car is detected
  if (slot1 || slot2 || slot3 || slot4) {
    if (lastPlate != "") {
      // Valid plate detected, allow parking
      int occupiedSlot = 0;
      if (slot1) occupiedSlot = 1;
      else if (slot2) occupiedSlot = 2;
      else if (slot3) occupiedSlot = 3;
      else if (slot4) occupiedSlot = 4;
      
      if (occupiedSlot > 0) {
        Serial.println("[DETECT] Car with plate " + lastPlate + " detected in slot " + String(occupiedSlot));
        updateSlotStatus(lastPlate, occupiedSlot);
        lastPlate = "";
      }
    } else {
      // No plate detected, reject parking
      int occupiedSlot = 0;
      if (slot1) occupiedSlot = 1;
      else if (slot2) occupiedSlot = 2;
      else if (slot3) occupiedSlot = 3;
      else if (slot4) occupiedSlot = 4;
      
      Serial.println("[REJECT] Vehicle without number plate detected in slot " + String(occupiedSlot) + " - PARKING DENIED");
      displayRejectionMessage(occupiedSlot);
    }
  } else if (lastPlate != "") {
    Serial.println("[WAIT] Plate set but no car detected in any slot yet");
  }
}

void checkParkingSlots() {
  static unsigned long lastSlotCheck = 0;
  if (millis() - lastSlotCheck < 3000) return;
  lastSlotCheck = millis();
  
  checkParkingSlotsImmediate();
}

void displayRejectionMessage(int slot) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("PARKING DENIED");
  display.println("==============");
  display.println("");
  display.println("No Number Plate");
  display.println("Detected!");
  display.println("");
  display.println("Please show your");
  display.println("number plate to");
  display.println("the camera first");
  display.display();
  
  // Auto clear after 5 seconds
  delay(5000);
  updateMainDisplay();
}

void updateSlotStatus(String plate, int slot) {
  HTTPClient http;
  
  String url = "http://" + String(serverIP) + ":5000/api/update_slot";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"plate\":\"" + plate + "\",\"slot\":" + String(slot) + "}";
  
  int httpCode = http.POST(payload);
  if (httpCode > 0) {
    Serial.println("[UPDATE] Slot updated: " + plate + " in slot " + String(slot));
  }
  
  http.end();
}