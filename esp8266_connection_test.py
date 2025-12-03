#!/usr/bin/env python3
"""
ESP8266 Gate Controller Connection Diagnostic Tool
Helps troubleshoot timeout issues with the gate controller
"""

import requests
import time
import socket
from datetime import datetime

# Configuration
ESP8266_ENTRY_IP = '10.137.170.52'  # ESP8266 Gate Controller IP
TIMEOUT = 5  # seconds

def test_ping(ip):
    """Test basic network connectivity"""
    try:
        # Try to connect to port 80
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2)
        result = sock.connect_ex((ip, 80))
        sock.close()
        return result == 0
    except:
        return False

def test_http_status(ip):
    """Test HTTP status endpoint"""
    try:
        url = f"http://{ip}/status"
        response = requests.get(url, timeout=TIMEOUT)
        return {
            'success': True,
            'status_code': response.status_code,
            'response': response.text,
            'time': response.elapsed.total_seconds()
        }
    except requests.exceptions.Timeout:
        return {'success': False, 'error': 'HTTP Timeout'}
    except requests.exceptions.ConnectionError:
        return {'success': False, 'error': 'Connection Error'}
    except Exception as e:
        return {'success': False, 'error': str(e)}

def test_gate_command(ip, action='open'):
    """Test gate control command"""
    try:
        url = f"http://{ip}/gate"
        params = {'action': action}
        response = requests.get(url, params=params, timeout=TIMEOUT)
        return {
            'success': True,
            'status_code': response.status_code,
            'response': response.text,
            'time': response.elapsed.total_seconds()
        }
    except requests.exceptions.Timeout:
        return {'success': False, 'error': 'Gate Command Timeout'}
    except requests.exceptions.ConnectionError:
        return {'success': False, 'error': 'Gate Connection Error'}
    except Exception as e:
        return {'success': False, 'error': str(e)}

def run_diagnostics():
    """Run complete diagnostic suite"""
    print("=" * 60)
    print("ESP8266 Gate Controller Connection Diagnostics")
    print("=" * 60)
    print(f"Target IP: {ESP8266_ENTRY_IP}")
    print(f"Timeout: {TIMEOUT} seconds")
    print(f"Test Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("-" * 60)
    
    # Test 1: Basic network connectivity
    print("1. Testing basic network connectivity...")
    ping_result = test_ping(ESP8266_ENTRY_IP)
    if ping_result:
        print("   [OK] Network connection: OK")
    else:
        print("   [FAIL] Network connection: FAILED")
        print("   -> Check if ESP8266 is powered on")
        print("   -> Verify IP address is correct")
        print("   -> Check WiFi connection")
    
    # Test 2: HTTP Status endpoint
    print("\n2. Testing HTTP status endpoint...")
    status_result = test_http_status(ESP8266_ENTRY_IP)
    if status_result['success']:
        print(f"   [OK] HTTP Status: OK ({status_result['time']:.2f}s)")
        print(f"   Response: {status_result['response']}")
    else:
        print(f"   [FAIL] HTTP Status: {status_result['error']}")
        if 'Timeout' in status_result['error']:
            print("   -> ESP8266 may be overloaded or unresponsive")
            print("   -> Try power cycling the device")
        
    # Test 3: Gate control commands
    print("\n3. Testing gate control commands...")
    
    # Test open command
    print("   Testing 'open' command...")
    open_result = test_gate_command(ESP8266_ENTRY_IP, 'open')
    if open_result['success']:
        print(f"   [OK] Gate Open: OK ({open_result['time']:.2f}s)")
        print(f"   Response: {open_result['response']}")
    else:
        print(f"   [FAIL] Gate Open: {open_result['error']}")
    
    time.sleep(1)
    
    # Test close command
    print("   Testing 'close' command...")
    close_result = test_gate_command(ESP8266_ENTRY_IP, 'close')
    if close_result['success']:
        print(f"   [OK] Gate Close: OK ({close_result['time']:.2f}s)")
        print(f"   Response: {close_result['response']}")
    else:
        print(f"   [FAIL] Gate Close: {close_result['error']}")
    
    # Test 4: Continuous monitoring
    print("\n4. Running continuous monitoring (10 tests)...")
    success_count = 0
    total_time = 0
    
    for i in range(10):
        print(f"   Test {i+1}/10...", end=" ")
        start_time = time.time()
        result = test_http_status(ESP8266_ENTRY_IP)
        end_time = time.time()
        
        if result['success']:
            success_count += 1
            total_time += (end_time - start_time)
            print(f"[OK] ({end_time - start_time:.2f}s)")
        else:
            print(f"[FAIL] {result['error']}")
        
        time.sleep(0.5)
    
    print(f"\n   Success Rate: {success_count}/10 ({success_count*10}%)")
    if success_count > 0:
        print(f"   Average Response Time: {total_time/success_count:.2f}s")
    
    # Summary and recommendations
    print("\n" + "=" * 60)
    print("DIAGNOSTIC SUMMARY")
    print("=" * 60)
    
    if ping_result and status_result['success'] and success_count >= 8:
        print("[HEALTHY] ESP8266 connection is HEALTHY")
        print("   The timeout issue may be intermittent or load-related.")
    elif ping_result and status_result['success']:
        print("[UNSTABLE] ESP8266 connection is UNSTABLE")
        print("   Connection works but has reliability issues.")
    elif ping_result:
        print("[DOWN] ESP8266 HTTP service is DOWN")
        print("   Network connection exists but web server not responding.")
    else:
        print("[OFFLINE] ESP8266 is OFFLINE")
        print("   No network connectivity detected.")
    
    print("\nRECOMMENDATIONS:")
    if not ping_result:
        print("* Check ESP8266 power supply")
        print("* Verify WiFi credentials and connection")
        print("* Confirm IP address configuration")
    elif not status_result['success']:
        print("* Power cycle the ESP8266")
        print("* Check for code crashes or infinite loops")
        print("* Monitor serial output for errors")
    elif success_count < 8:
        print("* Check WiFi signal strength")
        print("* Reduce server.handleClient() delay in ESP8266 code")
        print("* Add watchdog timer to prevent hangs")
        print("* Consider increasing timeout values in main.py")

if __name__ == "__main__":
    run_diagnostics()