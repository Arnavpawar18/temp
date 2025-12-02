import socket
import re

# Get current laptop IP
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect(("8.8.8.8", 80))
current_ip = s.getsockname()[0]
s.close()

print(f"Current laptop IP: {current_ip}")

# Update ESP32-CAM
with open('esp32_camera.ino', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()
content = re.sub(r'const char\* serverURL = "http://[\d.]+:5000/api/analyze-plate";', 
                 f'const char* serverURL = "http://{current_ip}:5000/api/analyze-plate";', content)
with open('esp32_camera.ino', 'w', encoding='utf-8') as f:
    f.write(content)
print("✓ Updated esp32_camera.ino")

# Update ESP2
with open('ESP2_parking_display.ino', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()
content = re.sub(r'const char\* serverIP = "[\d.]+";', 
                 f'const char* serverIP = "{current_ip}";', content)
with open('ESP2_parking_display.ino', 'w', encoding='utf-8') as f:
    f.write(content)
print("✓ Updated ESP2_parking_display.ino")

# Update connectivity test IP in ESP32-CAM
with open('esp32_camera.ino', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()
content = re.sub(r'testClient\.connect\("[\d.]+", 5000\)', 
                 f'testClient.connect("{current_ip}", 5000)', content)
with open('esp32_camera.ino', 'w', encoding='utf-8') as f:
    f.write(content)
print("✓ Updated connectivity test IPs")

print(f"\n✅ All files updated with server IP: {current_ip}")
print("Re-upload ESP32-CAM and ESP2 sketches to apply changes.")
