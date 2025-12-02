#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

const char* ssid = "parking";
const char* password = "0000011111";
const char* esp32camIP = "10.109.142.100";  // ESP32-CAM static IP

// Static IP configuration
IPAddress local_IP(10, 109, 142, 101);      // ESP8266 static IP
IPAddress gateway(10, 109, 142, 81);        // Router gateway
IPAddress subnet(255, 255, 255, 0);         // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);           // Google DNS
IPAddress secondaryDNS(8, 8, 4, 4);         // Google DNS

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

void setup() {
  Serial.begin(115200);
  pinMode(ENTRY_IR_PIN, INPUT_PULLUP);
  pinMode(EXIT_IR_PIN, INPUT_PULLUP);
  
  gateServo.attach(SERVO_PIN);
  gateServo.write(0);
  
  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Static IP failed");
  }
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("IP: " + WiFi.localIP().toString());
  
  server.on("/status", []() { server.send(200, "text/plain", "OK"); });
  server.on("/gate", handleGate);
  server.begin();
}

void loop() {
  server.handleClient();
  
  if (gateOpen && (millis() - gateOpenTime > 5000)) {
    gateServo.write(0);
    gateOpen = false;
  }
  
  if (millis() - lastTrigger < 2000) {
    delay(100);
    return;
  }
  
  if (!digitalRead(ENTRY_IR_PIN) && !entryIRState) {
    entryIRState = true;
    Serial.println("ENTRY");
    triggerCam("entry");
    lastTrigger = millis();
  } else if (digitalRead(ENTRY_IR_PIN)) {
    entryIRState = false;
  }
  
  if (!digitalRead(EXIT_IR_PIN) && !exitIRState) {
    exitIRState = true;
    Serial.println("EXIT");
    triggerCam("exit");
    lastTrigger = millis();
  } else if (digitalRead(EXIT_IR_PIN)) {
    exitIRState = false;
  }
  
  delay(100);
}

void triggerCam(String type) {
  WiFiClient client;
  if (client.connect(esp32camIP, 80)) {
    client.print("GET /capture?type=" + type + " HTTP/1.1\r\nHost: " + String(esp32camIP) + "\r\nConnection: close\r\n\r\n");
    unsigned long t = millis();
    while (client.available() == 0 && millis() - t < 5000);
    client.stop();
    Serial.println("Camera OK");
  }
}

void handleGate() {
  if (server.hasArg("action")) {
    if (server.arg("action") == "open") {
      gateServo.write(90);
      gateOpen = true;
      gateOpenTime = millis();
      server.send(200, "text/plain", "Opened");
      Serial.println("GATE OPEN");
    } else {
      gateServo.write(0);
      gateOpen = false;
      server.send(200, "text/plain", "Closed");
      Serial.println("GATE CLOSE");
    }
  }
}