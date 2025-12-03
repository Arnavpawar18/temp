#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

const char* ssid = "parking";
const char* password = "0000011111";
const char* esp32camIP = "10.137.170.68";

#define ENTRY_IR_PIN D1
#define EXIT_IR_PIN D2
#define SERVO_PIN D3

Servo gateServo;
ESP8266WebServer server(80);

bool entryIRState = false;
bool exitIRState = false;
bool gateOpen = false;
unsigned long lastTrigger = 0;
unsigned long gateOpenTime = 0;
String gateStatus = "Gate Closed";

// Servo positions - adjust these values if needed
const int GATE_CLOSED_POS = 10;   // Slightly above 0 to avoid servo strain
const int GATE_OPEN_POS = 170;    // Near max position for full opening

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== ESP8266 Gate Controller ===");
  Serial.println("Servo Pin: D3 (GPIO0)");
  Serial.println("Entry IR: D1 (GPIO5)");
  Serial.println("Exit IR: D2 (GPIO4)");
  Serial.println("===============================\n");
  
  pinMode(ENTRY_IR_PIN, INPUT_PULLUP);
  pinMode(EXIT_IR_PIN, INPUT_PULLUP);
  
  // Initialize servo with proper timing
  gateServo.attach(SERVO_PIN, 500, 2500);  // Extended pulse width range
  delay(500);
  
  // Move to closed position slowly
  Serial.println("Initializing gate to closed position...");
  moveServoSlowly(GATE_CLOSED_POS);
  gateOpen = false;
  gateStatus = "Gate Closed - System Ready";
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");
  Serial.println("IP: " + WiFi.localIP().toString());
  
  server.on("/status", []() { 
    server.send(200, "text/plain", gateStatus); 
  });
  server.on("/gate", handleGate);
  server.on("/test", handleServoTest);  // Test endpoint
  server.begin();
  
  Serial.println("ESP8266 Gate Controller Ready");
  Serial.println("Test servo: http://" + WiFi.localIP().toString() + "/test");
}

void loop() {
  server.handleClient();
  
  // Auto-close gate after 5 seconds
  if (gateOpen && (millis() - gateOpenTime > 5000)) {
    Serial.println("Auto-closing gate...");
    moveServoSlowly(GATE_CLOSED_POS);
    gateOpen = false;
    gateStatus = "Gate Closed - Auto Close";
    Serial.println("GATE AUTO-CLOSED");
  }
  
  // Prevent rapid triggering
  if (millis() - lastTrigger < 2000) {
    delay(100);
    return;
  }
  
  // Entry IR sensor
  if (!digitalRead(ENTRY_IR_PIN) && !entryIRState) {
    entryIRState = true;
    Serial.println("ENTRY IR TRIGGERED");
    triggerCam("entry");
    lastTrigger = millis();
  } else if (digitalRead(ENTRY_IR_PIN)) {
    entryIRState = false;
  }
  
  // Exit IR sensor
  if (!digitalRead(EXIT_IR_PIN) && !exitIRState) {
    exitIRState = true;
    Serial.println("EXIT IR TRIGGERED");
    triggerCam("exit");
    lastTrigger = millis();
  } else if (digitalRead(EXIT_IR_PIN)) {
    exitIRState = false;
  }
  
  delay(100);
}

void moveServoSlowly(int targetPos) {
  int currentPos = gateServo.read();
  Serial.println("Moving servo from " + String(currentPos) + " to " + String(targetPos));
  
  if (currentPos < targetPos) {
    // Move up
    for (int pos = currentPos; pos <= targetPos; pos += 2) {
      gateServo.write(pos);
      delay(20);  // Slow movement
    }
  } else {
    // Move down
    for (int pos = currentPos; pos >= targetPos; pos -= 2) {
      gateServo.write(pos);
      delay(20);  // Slow movement
    }
  }
  
  gateServo.write(targetPos);  // Ensure final position
  delay(500);  // Let servo settle
  Serial.println("Servo moved to position: " + String(targetPos));
}

void triggerCam(String type) {
  WiFiClient client;
  if (client.connect(esp32camIP, 80)) {
    client.print("GET /capture?type=" + type + " HTTP/1.1\r\nHost: " + String(esp32camIP) + "\r\nConnection: close\r\n\r\n");
    unsigned long t = millis();
    while (client.available() == 0 && millis() - t < 5000);
    client.stop();
    Serial.println("Camera triggered for " + type);
  } else {
    Serial.println("Failed to connect to camera");
  }
}

void handleGate() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    Serial.println("Gate command received: " + action);
    
    if (action == "open") {
      Serial.println("OPENING GATE...");
      moveServoSlowly(GATE_OPEN_POS);
      gateOpen = true;
      gateOpenTime = millis();
      gateStatus = "Gate Opened";
      server.send(200, "text/plain", "Opened");
      Serial.println("GATE OPENED SUCCESSFULLY");
    } else if (action == "close") {
      Serial.println("CLOSING GATE...");
      moveServoSlowly(GATE_CLOSED_POS);
      gateOpen = false;
      gateStatus = "Gate Closed";
      server.send(200, "text/plain", "Closed");
      Serial.println("GATE CLOSED SUCCESSFULLY");
    } else {
      server.send(400, "text/plain", "Invalid action");
    }
  } else {
    server.send(200, "text/plain", gateStatus);
  }
}

void handleServoTest() {
  Serial.println("=== SERVO TEST STARTED ===");
  
  // Test sequence
  Serial.println("1. Moving to closed position...");
  moveServoSlowly(GATE_CLOSED_POS);
  delay(1000);
  
  Serial.println("2. Moving to open position...");
  moveServoSlowly(GATE_OPEN_POS);
  delay(1000);
  
  Serial.println("3. Moving back to closed position...");
  moveServoSlowly(GATE_CLOSED_POS);
  
  Serial.println("=== SERVO TEST COMPLETED ===");
  server.send(200, "text/plain", "Servo test completed - check serial monitor");
}