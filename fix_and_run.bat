@echo off
echo Installing Flask-CORS...
pip install Flask-CORS==4.0.0

echo.
echo Starting Smart Parking System Server...
echo Make sure your ESP devices are connected and configured with these IPs:
echo - ESP32-CAM: 10.137.170.68
echo - ESP8266 Entry Controller: 10.137.170.69  
echo - ESP32 Display Controller: 10.137.170.70
echo - Server: 10.137.170.161
echo.

python main.py
pause