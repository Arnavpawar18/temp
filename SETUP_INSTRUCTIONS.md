# Smart Parking System - Complete Setup Guide

## 🚀 System Overview
- **Flask Server**: Number plate recognition + MongoDB database
- **ESP32-CAM**: Camera module for capturing vehicle images
- **ESP8266**: Main controller with IR sensors, servo gate, OLED display
- **Hardware**: Entry/Exit IR sensors, 4 parking slot sensors, servo motor, OLED

---

## 📋 Prerequisites

### Software Requirements
1. **Python 3.7+** with pip
2. **Arduino IDE** with ESP32/ESP8266 board support
3. **MongoDB Atlas** account (already configured)

### Hardware Requirements
1. **ESP32-CAM** module
2. **ESP8266 NodeMCU** or Wemos D1 Mini
3. **6x IR Sensors** (2 for entry/exit, 4 for parking slots)
4. **SG90 Servo Motor** for gate
5. **0.96" OLED Display** (SSD1306, I2C)
6. **Jumper wires** and breadboard

---

## 🔧 Step 1: Server Setup

### 1.1 Install Python Dependencies
```bash
cd numberplate
pip install -r requirements.txt
```

### 1.2 Configure IP Addresses
Edit `main.py` and update these IPs to match your network:
```python
ESP32_CAM_IP = '192.168.1.100'  # Your ESP32-CAM IP
ESP8266_IP = '192.168.1.101'    # Your ESP8266 IP
```

### 1.3 Start the Server
```bash
python main.py
```
✅ Server should start on `http://localhost:5000`

---

## 🔧 Step 2: ESP32-CAM Setup

### 2.1 Install Arduino Libraries
In Arduino IDE, install:
- **ESP32** board package
- **ArduinoJson** library

### 2.2 Configure ESP32-CAM Code
Edit `esp32_camera.ino`:
```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverURL = "http://192.168.1.101:5000/api/analyze-plate";
```

### 2.3 Upload Code
1. Connect ESP32-CAM to computer via FTDI
2. Select **AI Thinker ESP32-CAM** board
3. Upload `esp32_camera.ino`
4. Note the IP address from Serial Monitor

---

## 🔧 Step 3: ESP8266 Setup

### 3.1 Install Arduino Libraries
In Arduino IDE, install:
- **ESP8266** board package
- **Servo** library
- **Adafruit SSD1306** library
- **ArduinoJson** library

### 3.2 Configure ESP8266 Code
Edit `esp8266_controller.ino`:
```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverIP = "192.168.1.101";      // Your computer IP
const char* esp32camIP = "192.168.1.100";    // ESP32-CAM IP
```

### 3.3 Upload Code
1. Connect ESP8266 to computer
2. Select **NodeMCU 1.0** or your ESP8266 board
3. Upload `esp8266_controller.ino`
4. Note the IP address from Serial Monitor

---

## 🔌 Step 4: Hardware Connections

### ESP8266 Pin Connections:
```
Component          ESP8266 Pin    GPIO
Entry IR Sensor    D1             GPIO5
Exit IR Sensor     D2             GPIO4
Servo Motor        D3             GPIO0
OLED SDA          D4             GPIO2
OLED SCL          D5             GPIO14
Slot 1 IR         D6             GPIO12
Slot 2 IR         D7             GPIO13
Slot 3 IR         D8             GPIO15
Slot 4 IR         RX             GPIO3
```

### Power Connections:
- **All IR Sensors**: VCC to 3.3V, GND to GND
- **Servo Motor**: VCC to 5V, GND to GND
- **OLED Display**: VCC to 3.3V, GND to GND

---

## 🔧 Step 5: Network Configuration

### 5.1 Find Device IPs
After uploading code, check Serial Monitor for IP addresses:
- ESP32-CAM: `192.168.1.100` (example)
- ESP8266: `192.168.1.101` (example)
- Your Computer: Check with `ipconfig` (Windows) or `ifconfig` (Mac/Linux)

### 5.2 Update All IP References
Update these files with actual IPs:
1. **main.py**: Update `ESP32_CAM_IP` and `ESP8266_IP`
2. **esp32_camera.ino**: Update `serverURL`
3. **esp8266_controller.ino**: Update `serverIP` and `esp32camIP`

---

## 🧪 Step 6: Testing

### 6.1 Test Server
```bash
python test.py
```
Should show: `✓ Server is running`

### 6.2 Test ESP32-CAM
Visit: `http://ESP32_CAM_IP/capture?type=entry`
Should respond: `OK`

### 6.3 Test ESP8266
Visit: `http://ESP8266_IP/gate?action=open`
Should open the servo gate

### 6.4 Test Complete Flow
1. Trigger entry IR sensor (wave hand)
2. ESP32-CAM should capture image
3. Server should process number plate
4. Gate should open if successful
5. OLED should show welcome message

---

## 🚗 Step 7: System Operation

### Entry Process:
1. Car approaches → **Entry IR triggered**
2. ESP8266 → ESP32-CAM → **Captures image**
3. ESP32-CAM → Server → **Gemini analyzes plate**
4. Server → Database → **Checks if car already parked**
5. If allowed → **Gate opens** → Car enters
6. Car parks → **Slot IR detects** → **Database updated**

### Exit Process:
1. Car approaches → **Exit IR triggered**
2. ESP32-CAM → **Captures image**
3. Server → **Calculates bill** (₹10/min, min ₹10)
4. ESP8266 → **OLED displays bill**
5. **Gate opens** → Car exits

---

## 🛠️ Troubleshooting

### Common Issues:

**Server won't start:**
- Check MongoDB connection
- Install missing dependencies: `pip install pymongo requests`

**ESP devices won't connect:**
- Check WiFi credentials
- Ensure all devices on same network
- Check IP addresses in code

**Camera not working:**
- Check ESP32-CAM wiring
- Ensure proper power supply (5V recommended)
- Check camera module connection

**Gate not opening:**
- Check servo connections
- Ensure 5V power for servo
- Test servo manually: `servo.write(90)`

**OLED not displaying:**
- Check I2C connections (SDA/SCL)
- Verify OLED address (usually 0x3C)
- Test with simple OLED example

---

## 📊 Database Structure

MongoDB collection `cars`:
```json
{
  "plate": "ABC123",
  "entry_time": "2024-01-01T10:00:00Z",
  "exit_time": null,
  "slot": 2,
  "booking_id": "BK20240101100000"
}
```

---

## 🔧 Configuration Summary

**Update these before deployment:**
1. WiFi credentials in both ESP codes
2. IP addresses in all 3 files (main.py, both .ino files)
3. MongoDB URI (already configured)
4. Gemini API key (already configured)

**Default Settings:**
- Parking rate: ₹10/minute
- Minimum charge: ₹10
- Gate open duration: 5 seconds
- Max retry attempts: 5

---

## 📞 Support

If you encounter issues:
1. Check Serial Monitor outputs
2. Verify all connections
3. Test each component individually
4. Ensure all IPs are correctly configured

**System is ready for deployment!** 🎉