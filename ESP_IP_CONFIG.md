# ESP Device IP Configuration

## Current Network Configuration
- **Server (Your Laptop)**: 10.137.170.161:5000
- **ESP32-CAM**: 10.137.170.68
- **ESP8266 Entry Controller**: 10.137.170.69
- **ESP32 Display Controller**: 10.137.170.70

## Required Changes to ESP Code

### 1. ESP32-CAM (esp32_camera.ino)
```cpp
// Update server URL
const char* serverURL = "http://10.137.170.161:5000/api/analyze-plate";

// Add static IP configuration in setup()
WiFi.config(IPAddress(10, 137, 170, 68), 
           IPAddress(10, 137, 170, 1), 
           IPAddress(255, 255, 255, 0));
```

### 2. ESP8266 Entry Controller (ESP1_ESP8266_FINAL.ino)
```cpp
// Add static IP configuration in setup()
WiFi.config(IPAddress(10, 137, 170, 69), 
           IPAddress(10, 137, 170, 1), 
           IPAddress(255, 255, 255, 0));
```

### 3. ESP32 Display Controller (ESP2_parking_display.ino)
```cpp
// Update server IP
const char* serverIP = "10.137.170.161";

// Add static IP configuration in setup()
WiFi.config(IPAddress(10, 137, 170, 70), 
           IPAddress(10, 137, 170, 1), 
           IPAddress(255, 255, 255, 0));
```

## Troubleshooting Steps

1. **Check Network Connection**: All devices must be on the same WiFi network "parking"
2. **Verify Server IP**: Run `ipconfig` on your laptop to confirm IP is 10.137.170.161
3. **Test Connectivity**: Ping each ESP device from your laptop
4. **Check Firewall**: Ensure Windows Firewall allows port 5000
5. **Monitor Serial Output**: Check ESP serial monitors for connection status

## Admin Panel Gate Control Fix

The admin panel should now work with proper authentication and error handling. Make sure:
1. Login to admin panel first
2. ESP8266 Entry Controller is online at 10.137.170.69
3. Check browser console for any JavaScript errors