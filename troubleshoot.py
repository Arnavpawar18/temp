#!/usr/bin/env python3
"""
Smart Parking System Troubleshooting Script
"""
import requests
import socket
import subprocess
import sys

# Configuration
ESP_DEVICES = {
    "ESP32-CAM": "10.137.170.68",
    "ESP8266 Entry Controller": "10.137.170.69", 
    "ESP32 Display Controller": "10.137.170.70"
}
SERVER_IP = "10.137.170.161"
SERVER_PORT = 5000

def get_local_ip():
    """Get the local IP address"""
    try:
        # Connect to a remote server to determine local IP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except:
        return "Unknown"

def ping_device(ip):
    """Ping a device to check connectivity"""
    try:
        result = subprocess.run(['ping', '-n', '1', ip], 
                              capture_output=True, text=True, timeout=5)
        return result.returncode == 0
    except:
        return False

def check_http_endpoint(ip, port=80, endpoint="/status"):
    """Check if HTTP endpoint is accessible"""
    try:
        response = requests.get(f"http://{ip}:{port}{endpoint}", timeout=3)
        return response.status_code == 200
    except:
        return False

def check_server_status():
    """Check if Flask server is running"""
    try:
        response = requests.get(f"http://{SERVER_IP}:{SERVER_PORT}/status", timeout=3)
        return response.status_code == 200
    except:
        return False

def main():
    print("=" * 60)
    print("Smart Parking System Troubleshooting")
    print("=" * 60)
    
    # Check local IP
    local_ip = get_local_ip()
    print(f"Your computer's IP: {local_ip}")
    
    if local_ip != SERVER_IP:
        print(f"⚠️  WARNING: Your IP ({local_ip}) doesn't match configured server IP ({SERVER_IP})")
        print("   Update main.py or ESP device configurations accordingly")
    else:
        print("✅ IP configuration matches")
    
    print()
    
    # Check Flask server
    print("Checking Flask Server...")
    if check_server_status():
        print(f"✅ Flask server is running at {SERVER_IP}:{SERVER_PORT}")
    else:
        print(f"❌ Flask server is NOT accessible at {SERVER_IP}:{SERVER_PORT}")
        print("   - Make sure you ran 'python main.py'")
        print("   - Check Windows Firewall settings")
        print("   - Verify Flask-CORS is installed")
    
    print()
    
    # Check ESP devices
    print("Checking ESP Devices...")
    for device_name, device_ip in ESP_DEVICES.items():
        print(f"\n{device_name} ({device_ip}):")
        
        # Ping test
        if ping_device(device_ip):
            print(f"  ✅ Ping successful")
            
            # HTTP test
            if check_http_endpoint(device_ip):
                print(f"  ✅ HTTP endpoint accessible")
            else:
                print(f"  ❌ HTTP endpoint not accessible")
                print(f"     - Check if device web server is running")
                print(f"     - Verify device code has /status endpoint")
        else:
            print(f"  ❌ Ping failed")
            print(f"     - Check if device is powered on")
            print(f"     - Verify WiFi connection")
            print(f"     - Confirm IP address configuration")
    
    print()
    print("=" * 60)
    print("Troubleshooting Complete")
    print("=" * 60)
    
    # Recommendations
    print("\nRecommendations:")
    print("1. Ensure all devices are on the same WiFi network 'parking'")
    print("2. Upload updated ESP code with correct IP configurations")
    print("3. Install Flask-CORS: pip install Flask-CORS==4.0.0")
    print("4. Run server: python main.py")
    print("5. Access admin panel: http://10.137.170.161:5000/admin")
    print("   Username: admin, Password: parking123")

if __name__ == "__main__":
    main()