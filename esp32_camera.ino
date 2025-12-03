#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "parking";
const char* password = "0000011111";

// Server IP (your laptop)
const char* serverURL = "http://10.137.170.161:5000/api/analyze-plate";

// Camera pins (AI-Thinker ESP32-CAM)a
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_LED_PIN      4

WebServer server(80);
bool useFlash = false;
String captureType = "entry";  // "entry" or "exit"

void setup() {
  Serial.begin(115200);
  
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  
  // Use DHCP to get IP on same subnet as laptop
  // Removed static IP configuration
  
  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  esp_camera_init(&config);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
  Serial.println("Gateway: " + WiFi.gatewayIP().toString());
  Serial.println("Server: " + String(serverURL));
  
  // Test server connectivity on boot
  Serial.println("\nTesting server connection...");
  WiFiClient testClient;
  if (testClient.connect("10.137.170.161", 5000)) {
    Serial.println("✅ Server is reachable!");
    testClient.stop();
  } else {
    Serial.println("❌ Cannot reach server at 10.137.170.161:5000");
    Serial.println("Check: 1) Flask running 2) Firewall 3) Same network");
  }
  
  // Setup web server for capture triggers
  server.on("/capture", handleCapture);
  server.begin();
}

void loop() {
  server.handleClient();
  // Only capture when triggered - no auto capture
  delay(100);
}

void handleCapture() {
  // Get parameters from ESP8266
  if (server.hasArg("flash")) {
    useFlash = server.arg("flash") == "1";
  }
  
  if (server.hasArg("type")) {
    captureType = server.arg("type");  // "entry" or "exit"
  }
  
  Serial.println("Capture triggered - Type: " + captureType + ", Flash: " + (useFlash ? "ON" : "OFF"));
  
  server.send(200, "text/plain", "OK");
  
  // Capture immediately when triggered
  delay(100);
  captureAndSend();
}

void captureAndSend() {
  Serial.println("\n=== STARTING IMAGE CAPTURE ===");
  Serial.println("Flash: " + String(useFlash ? "ON" : "OFF"));
  Serial.println("Type: " + captureType);
  
  if (useFlash) {
    Serial.println("Turning on flash...");
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(200);
  }
  
  Serial.println("Capturing image...");
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ CAPTURE FAILED!");
    digitalWrite(FLASH_LED_PIN, LOW);
    return;
  }
  
  Serial.println("✅ Image captured successfully");
  Serial.println("Image size: " + String(fb->len) + " bytes");
  
  digitalWrite(FLASH_LED_PIN, LOW);
  
  Serial.println("\n=== SENDING TO SERVER ===");
  Serial.println("Server URL: " + String(serverURL));
  
  // Test connectivity first
  WiFiClient testClient;
  Serial.println("Testing connection to 10.137.170.161:5000...");
  if (!testClient.connect("10.137.170.161", 5000)) {
    Serial.println("❌ Cannot connect to server! Check:");
    Serial.println("  1. Flask server running?");
    Serial.println("  2. Server IP = 10.137.170.161?");
    Serial.println("  3. Windows Firewall port 5000 open?");
    esp_camera_fb_return(fb);
    return;
  }
  testClient.stop();
  Serial.println("✅ Server reachable, sending image...");
  
  HTTPClient http;
  http.begin(serverURL);
  http.setTimeout(30000);  // 30 second timeout for AI processing
  
  String boundary = "----ESP32Boundary";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);
  Serial.println("Content-Type: " + contentType);
  
  String body = "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"image\"; filename=\"esp32.jpg\"\r\n";
  body += "Content-Type: image/jpeg\r\n\r\n";
  
  // Add type parameter to the request
  String typeField = "\r\n--" + boundary + "\r\n";
  typeField += "Content-Disposition: form-data; name=\"type\"\r\n\r\n";
  typeField += captureType;
  
  String footer = "\r\n--" + boundary + "--\r\n";
  
  int totalLen = body.length() + fb->len + typeField.length() + footer.length();
  Serial.println("Total payload size: " + String(totalLen) + " bytes");
  
  uint8_t* postData = (uint8_t*)malloc(totalLen);
  if (!postData) {
    Serial.println("❌ Memory allocation failed!");
    esp_camera_fb_return(fb);
    return;
  }
  
  int offset = 0;
  memcpy(postData + offset, body.c_str(), body.length());
  offset += body.length();
  
  memcpy(postData + offset, fb->buf, fb->len);
  offset += fb->len;
  
  memcpy(postData + offset, typeField.c_str(), typeField.length());
  offset += typeField.length();
  
  memcpy(postData + offset, footer.c_str(), footer.length());
  
  Serial.println("Payload prepared, sending POST request...");
  
  int response = http.POST(postData, totalLen);
  
  Serial.println("\n=== SERVER RESPONSE ===");
  Serial.println("HTTP Code: " + String(response));
  
  if (response == -5) {
    Serial.println("⚠️ Connection refused - Server may be down or firewall blocking");
  } else if (response == -11) {
    Serial.println("⚠️ Read timeout - Server processing took too long");
  } else if (response < 0) {
    Serial.println("⚠️ Network error occurred");
  }
  
  if (response > 0) {
    String result = http.getString();
    Serial.println("✅ Response received:");
    Serial.println(result);
    
    // Check if retry needed
    if (result.indexOf("retrying") > -1) {
      if (result.indexOf("\"flash\":true") > -1) {
        useFlash = true;
        Serial.println("🔦 Flash enabled for next capture");
      }
      Serial.println("🔄 Retry requested by server");
    } else if (result.indexOf("plate_text") > -1) {
      useFlash = false;
      Serial.println("✅ SUCCESS! Plate recognized");
    } else if (result.indexOf("error") > -1) {
      Serial.println("❌ Server returned error");
    }
  } else {
    Serial.println("❌ HTTP POST FAILED!");
    Serial.println("Error code: " + String(response));
  }
  
  http.end();
  esp_camera_fb_return(fb);
  free(postData);
  
  Serial.println("=== CAPTURE COMPLETE ===\n");
}