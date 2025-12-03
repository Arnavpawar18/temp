#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "parking";
const char* password = "0000011111";
const char* esp32camIP = "10.137.170.68";  // ESP32-CAM IP

#define ENTRY_IR_PIN D1
#define EXIT_IR_PIN D2
#define SERVO_PIN D3
#define LED_PIN LED_BUILTIN  // Built-in LED for status

Servo gateServo;
ESP8266WebServer server(80);

bool entryIRState = false;
bool exitIRState = false;
bool gateOpen = false;
unsigned long lastTrigger = 0;
unsigned long gateOpenTime = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWiFiCheck = 0;
String gateStatus = "Gate Closed";

// Connection monitoring
bool wifiConnected = false;
int connectionRetries = 0;
const int MAX_RETRIES = 3;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP8266 Gate Controller v2.0 ===");
  
  // Initialize pins
  pinMode(ENTRY_IR_PIN, INPUT_PULLUP);
  pinMode(EXIT_IR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially
  
  // Initialize servo
  gateServo.attach(SERVO_PIN);
  gateServo.write(0); // Closed position
  
  // Connect to WiFi with retry logic
  connectToWiFi();
  
  // Setup web server endpoints
  server.on("/status", handleStatus);
  server.on("/gate", handleGate);
  server.on("/health", handleHealth);
  server.begin();
  
  Serial.println("ESP8266 Gate Controller Ready");
  Serial.println("IP: " + WiFi.localIP().toString());
  blinkLED(3); // Success indication
}

void loop() {
  // Handle web server requests
  server.handleClient();
  
  // Check WiFi connection every 30 seconds
  if (millis() - lastWiFiCheck > 30000) {
    checkWiFiConnection();
    lastWiFiCheck = millis();
  }
  
  // Auto-close gate after timeout
  if (gateOpen && (millis() - gateOpenTime > 5000)) {
    closeGate();
  }
  
  // Prevent rapid IR triggers
  if (millis() - lastTrigger < 2000) {
    delay(50);
    return;
  }
  
  // Check IR sensors
  checkIRSensors();
  
  // Send heartbeat every 60 seconds
  if (millis() - lastHeartbeat > 60000) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }
  
  delay(50); // Reduced delay for better responsiveness
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
    
    // Blink LED during connection
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    digitalWrite(LED_PIN, LOW); // LED on when connected
    Serial.println("\nWiFi connected!");
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    digitalWrite(LED_PIN, HIGH); // LED off when disconnected
    Serial.println("\nWiFi connection failed!");
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("WiFi connection lost! Attempting reconnection...");
      wifiConnected = false;
      digitalWrite(LED_PIN, HIGH); // LED off
    }
    
    connectionRetries++;
    if (connectionRetries <= MAX_RETRIES) {
      WiFi.reconnect();
      delay(5000);
      
      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        connectionRetries = 0;
        digitalWrite(LED_PIN, LOW); // LED on
        Serial.println("WiFi reconnected!");
      }
    } else {
      // Max retries reached, restart ESP
      Serial.println("Max WiFi retries reached. Restarting...");
      ESP.restart();
    }
  } else {
    if (!wifiConnected) {
      wifiConnected = true;
      connectionRetries = 0;
      digitalWrite(LED_PIN, LOW); // LED on
      Serial.println("WiFi connection restored!");
    }
  }
}

void checkIRSensors() {
  // Entry sensor
  if (!digitalRead(ENTRY_IR_PIN) && !entryIRState) {
    entryIRState = true;
    Serial.println("ENTRY IR TRIGGERED");
    triggerCam("entry");
    lastTrigger = millis();
  } else if (digitalRead(ENTRY_IR_PIN)) {
    entryIRState = false;
  }
  
  // Exit sensor
  if (!digitalRead(EXIT_IR_PIN) && !exitIRState) {
    exitIRState = true;
    Serial.println("EXIT IR TRIGGERED");
    triggerCam("exit");
    lastTrigger = millis();
  } else if (digitalRead(EXIT_IR_PIN)) {
    exitIRState = false;
  }
}

void triggerCam(String type) {
  if (!wifiConnected) {
    Serial.println("Cannot trigger camera - WiFi disconnected");
    return;
  }
  
  WiFiClient client;
  Serial.println("Triggering camera for " + type + "...");
  
  if (client.connect(esp32camIP, 80)) {
    String request = "GET /capture?type=" + type + " HTTP/1.1\r\n";
    request += "Host: " + String(esp32camIP) + "\r\n";
    request += "Connection: close\r\n\r\n";
    
    client.print(request);
    
    // Wait for response with timeout
    unsigned long timeout = millis() + 3000;
    while (client.available() == 0 && millis() < timeout) {
      delay(10);
    }
    
    client.stop();
    Serial.println("Camera triggered successfully");
  } else {
    Serial.println("Failed to connect to camera");
  }
}

void handleStatus() {
  String status = "ESP8266 Gate Controller\n";
  status += "Status: " + gateStatus + "\n";
  status += "WiFi: " + String(wifiConnected ? "Connected" : "Disconnected") + "\n";
  status += "IP: " + WiFi.localIP().toString() + "\n";
  status += "Uptime: " + String(millis() / 1000) + "s\n";
  status += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes";
  
  server.send(200, "text/plain", status);
}

void handleHealth() {
  // Quick health check endpoint
  server.send(200, "application/json", 
    "{\"status\":\"ok\",\"wifi\":" + String(wifiConnected ? "true" : "false") + 
    ",\"gate\":\"" + gateStatus + "\",\"uptime\":" + String(millis() / 1000) + "}");
}

void handleGate() {
  if (!server.hasArg("action")) {
    server.send(400, "text/plain", "Missing action parameter");
    return;
  }
  
  String action = server.arg("action");
  Serial.println("Gate command received: " + action);
  
  if (action == "open") {
    openGate();
    server.send(200, "text/plain", gateStatus);
  } else if (action == "close") {
    closeGate();
    server.send(200, "text/plain", gateStatus);
  } else {
    server.send(400, "text/plain", "Invalid action: " + action);
  }
}

void openGate() {
  gateServo.write(90); // Open position
  gateOpen = true;
  gateOpenTime = millis();
  gateStatus = "Gate Opening - Plate Detected";
  Serial.println("GATE OPENED");
  blinkLED(1); // Single blink for gate open
}

void closeGate() {
  gateServo.write(0); // Closed position
  gateOpen = false;
  gateStatus = "Gate Closed";
  Serial.println("GATE CLOSED");
}

void sendHeartbeat() {
  if (!wifiConnected) return;
  
  // Optional: Send heartbeat to main server
  // This can help the main server know the ESP8266 is alive
  Serial.println("Heartbeat - System OK");
}

void blinkLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
}