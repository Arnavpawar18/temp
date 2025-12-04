import json
import os
import base64
import time
from io import BytesIO
import requests
from datetime import datetime

import google.genai as genai
from flask import Flask, jsonify, request, send_file, send_from_directory, render_template, session, redirect, url_for
from flask_cors import CORS
from PIL import Image
from pymongo import MongoClient
from urllib.parse import quote_plus

# Configuration
API_KEY = ''
ESP32_CAM_IP = '10.80.93.224'     # ESP32-CAM static IP
ESP8266_ENTRY_IP = '10.80.93.52' # ESP1 ESP8266 static IP
ESP8266_DISPLAY_IP = '10.80.93.52' # ESP2 ZY-ESP32 static IP
MAX_RETRIES = 5
PER_MINUTE_RATE = 10  # ₹10 per minute
MINIMUM_CHARGE = 10   # ₹10 minimum

# MongoDB connection
MONGO_URI = "mongodb+srv://pawararnav232:pawararnav1234@parkingsystem.nxycox7.mongodb.net/?retryWrites=true&w=majority&appName=parkingSystem"
try:
    import ssl
    ssl_context = ssl.create_default_context()
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE
    client = MongoClient(MONGO_URI, tlsAllowInvalidCertificates=True)
except:
    client = MongoClient(MONGO_URI)
db = client.parking_system
cars_collection = db.cars

# Test connection on startup
try:
    client.admin.command('ping')
    print("✅ MongoDB connected successfully")
except Exception as e:
    print(f"❌ MongoDB connection failed: {e}")

ai = genai.Client(api_key=API_KEY)
app = Flask(__name__)
CORS(app)  # Enable CORS for all routes
app.secret_key = 'parking_system_secret_key_2024'  # Change this to a random secret key
app.config['SEND_FILE_MAX_AGE_DEFAULT'] = 0
retry_count = {}

# Admin credentials
ADMIN_USERNAME = 'admin'
ADMIN_PASSWORD = 'parking123'


@app.route("/")
def index():
    return render_template('dashboard.html')

@app.route("/about")
def about():
    return render_template('about.html')

@app.route("/admin")
def admin():
    if not session.get('admin_logged_in'):
        return redirect(url_for('admin_login'))
    return render_template('admin.html')

@app.route("/admin/login")
def admin_login():
    return render_template('login.html')

@app.route("/api/admin_login", methods=["POST"])
def admin_login_post():
    data = request.get_json()
    username = data.get('username')
    password = data.get('password')
    
    if username == ADMIN_USERNAME and password == ADMIN_PASSWORD:
        session['admin_logged_in'] = True
        return jsonify({'success': True})
    else:
        return jsonify({'success': False, 'message': 'Invalid username or password'})

@app.route("/admin/logout")
def admin_logout():
    session.pop('admin_logged_in', None)
    return redirect(url_for('index'))

@app.route("/api/control_gate", methods=["POST"])
def control_gate():
    """Admin gate control"""
    # Check admin authentication
    if not session.get('admin_logged_in'):
        return jsonify({'success': False, 'message': 'Unauthorized access'})
    
    data = request.get_json()
    gate_type = data.get('type')  # 'entry' or 'exit'
    action = data.get('action')  # 'open' or 'close'
    
    print(f"🔐 Admin gate control: {gate_type} gate {action}")
    
    gate_success = trigger_esp8266_gate(action)
    if gate_success:
        print(f"✅ Admin gate command successful: {action}")
        return jsonify({'success': True, 'message': f'{gate_type.title()} gate {action}ed successfully'})
    else:
        print(f"❌ Admin gate command failed: {action}")
        return jsonify({'success': False, 'message': 'Gate controller timeout - check ESP8266 connection. Try running esp8266_connection_test.py for diagnostics.'})

@app.route("/api/admin_stats")
def admin_stats():
    """Get today's statistics for admin dashboard"""
    today_start = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
    
    # Today's entries
    today_entries = cars_collection.count_documents({
        'entry_time': {'$gte': today_start}
    })
    
    # Currently parked
    currently_parked = cars_collection.count_documents({
        'exit_time': None,
        'entry_time': {'$ne': None}
    })
    
    # Exited today
    exited_today = cars_collection.count_documents({
        'exit_time': {'$gte': today_start}
    })
    
    # Revenue today
    exited_cars = list(cars_collection.find({
        'exit_time': {'$gte': today_start}
    }))
    revenue_today = sum([calculate_bill(car['entry_time']) for car in exited_cars if car.get('entry_time')])
    
    return jsonify({
        'today_entries': today_entries,
        'currently_parked': currently_parked,
        'exited_today': exited_today,
        'revenue_today': revenue_today
    })

@app.route("/api/dashboard")
def dashboard():
    """Get current parking status"""
    parked_cars = list(cars_collection.find({"exit_time": None}))
    occupied_slots = [car['slot'] for car in parked_cars if car.get('slot')]
    
    slots = []
    for i in range(1, 5):
        slot_car = next((car for car in parked_cars if car.get('slot') == i), None)
        slots.append({
            'number': i,
            'status': 'occupied' if i in occupied_slots else 'available',
            'plate': slot_car['plate'] if slot_car else None
        })
    
    parked_list = []
    for car in parked_cars:
        duration = (datetime.now() - car['entry_time']).total_seconds() / 60
        parked_list.append({
            'plate': car['plate'],
            'slot': car.get('slot'),
            'entry_time': car['entry_time'].strftime('%Y-%m-%d %H:%M:%S'),
            'duration': f"{int(duration)} min"
        })
    
    return jsonify({
        'available': 4 - len(occupied_slots),
        'occupied': len(occupied_slots),
        'slots': slots,
        'parked_cars': parked_list
    })

@app.route("/api/book", methods=["POST"])
def book_slot():
    """Book a parking slot"""
    data = request.get_json()
    plate = data.get('plate')
    name = data.get('name')
    phone = data.get('phone')
    
    if not plate or not name or not phone:
        return jsonify({'success': False, 'message': 'All fields required'})
    
    # Check if already booked
    existing = cars_collection.find_one({'plate': plate, 'exit_time': None})
    if existing:
        return jsonify({'success': False, 'message': 'Vehicle already has active booking'})
    
    # Check available slots
    occupied = cars_collection.count_documents({'exit_time': None, 'slot': {'$ne': None}})
    if occupied >= 4:
        return jsonify({'success': False, 'message': 'No slots available'})
    
    # Create booking
    booking = {
        'plate': plate.upper(),
        'name': name,
        'phone': phone,
        'booking_time': datetime.now(),
        'entry_time': None,
        'exit_time': None,
        'slot': None,
        'booking_id': f"BK{datetime.now().strftime('%Y%m%d%H%M%S')}"
    }
    cars_collection.insert_one(booking)
    
    return jsonify({'success': True, 'message': f'Slot booked! Booking ID: {booking["booking_id"]}'})


def trigger_esp32_capture(use_flash=False):
    """Send trigger to ESP32-CAM to capture new image"""
    try:
        url = f"http://{ESP32_CAM_IP}/capture"
        params = {'flash': '1' if use_flash else '0'}
        requests.get(url, params=params, timeout=5)
    except:
        pass

def trigger_esp8266_gate(action):
    """Send gate control signal to ESP8266 Entry/Exit Controller with retry logic"""
    max_retries = 3
    for attempt in range(max_retries):
        try:
            url = f"http://{ESP8266_ENTRY_IP}/gate"
            params = {'action': action}  # 'open' or 'close'
            response = requests.get(url, params=params, timeout=5)  # Reduced timeout
            if response.status_code == 200:
                print(f"🚪 Gate command sent: {action} (attempt {attempt + 1})")
                return True
            else:
                print(f"❌ Gate command failed: HTTP {response.status_code} (attempt {attempt + 1})")
        except requests.exceptions.Timeout:
            print(f"⏰ Gate command timeout (attempt {attempt + 1}/{max_retries})")
            if attempt < max_retries - 1:
                time.sleep(1)  # Wait before retry
        except requests.exceptions.ConnectionError:
            print(f"🔌 Gate connection error (attempt {attempt + 1}/{max_retries})")
            if attempt < max_retries - 1:
                time.sleep(1)
        except Exception as e:
            print(f"❌ Gate command error: {e} (attempt {attempt + 1}/{max_retries})")
            if attempt < max_retries - 1:
                time.sleep(1)
    
    print(f"❌ Gate command failed after {max_retries} attempts")
    return False

def calculate_bill(entry_time):
    """Calculate parking bill"""
    exit_time = datetime.now()
    duration = (exit_time - entry_time).total_seconds() / 60  # minutes
    
    if duration <= 1:
        return MINIMUM_CHARGE
    else:
        return MINIMUM_CHARGE + (int(duration - 1) * PER_MINUTE_RATE)

@app.route("/api/analyze-plate", methods=["POST"])
def analyze_plate():
    print(f"\n=== IMAGE RECEIVED ===")
    print(f"Request from: {request.remote_addr}")
    print(f"Request method: {request.method}")
    print(f"Content type: {request.content_type}")
    
    if API_KEY == 'TODO':
        return jsonify({"error": "API key not configured"})
    
    try:
        # Handle image from ESP32 camera
        if 'image' not in request.files:
            print("❌ ERROR: No image in request.files")
            print(f"Available files: {list(request.files.keys())}")
            return jsonify({"error": "No image provided"})
        
        image_file = request.files['image']
        if image_file.filename == '':
            print("❌ ERROR: Empty filename")
            return jsonify({"error": "No image selected"})
        
        print(f"✅ Image received: {image_file.filename}")
        
        # Get entry/exit type from form data
        entry_type = request.form.get('type', 'entry')  # 'entry' or 'exit'
        print(f"Entry type: {entry_type}")
        
        # Get client IP for retry tracking
        client_ip = request.remote_addr
        current_retry = retry_count.get(client_ip, 0)
        print(f"Retry count: {current_retry}")
        
        # Convert image to base64
        image_data = image_file.read()
        print(f"Image data size: {len(image_data)} bytes")
        
        # Save image for debugging
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        debug_folder = 'debug_images'
        if not os.path.exists(debug_folder):
            os.makedirs(debug_folder)
        
        image_filename = f"{debug_folder}/{entry_type}_{timestamp}_{client_ip.replace('.', '_')}.jpg"
        with open(image_filename, 'wb') as f:
            f.write(image_data)
        print(f"💾 Image saved: {image_filename}")
        
        image_base64 = base64.b64encode(image_data).decode('utf-8')
        print(f"Base64 encoded size: {len(image_base64)} characters")
        
        # Prepare content for Gemini API
        contents = [{
            "role": "user",
            "parts": [
                {
                    "inline_data": {
                        "mime_type": "image/jpeg",
                        "data": image_base64
                    }
                },
                {
                    "text": "You are a vehicle number plate recognition system. Look for a rectangular license plate on a vehicle.\n\nINDIAN PLATE FORMAT: 2 letters + 2 digits + 1-2 letters + 4 digits (e.g., MH12AB1234)\n\nINSTRUCTIONS:\n1. If you see a clear readable plate, extract and return ONLY the alphanumeric text (remove spaces/hyphens, uppercase)\n2. If plate exists but is blurry/unclear, return: BLUR_ERROR\n3. If NO vehicle or plate visible, return: NO_PLATE\n\nEXAMPLES:\n- Clear plate 'MH 12 AB 1234' → MH12AB1234\n- Blurry plate → BLUR_ERROR\n- Empty image → NO_PLATE\n\nRespond with ONLY the plate text or status code."
                }
            ]
        }]
        
        # Call Gemini API
        print("🤖 Calling Gemini API...")
        response = ai.models.generate_content(
            model="gemini-2.0-flash",
            contents=contents
        )
        
        result_text = response.text.strip()
        print(f"🤖 Gemini response: {result_text}")
        
        # Check if image is blurry or has error
        if "BLUR_ERROR" in result_text or "blur" in result_text.lower() or "unclear" in result_text.lower():
            retry_count[client_ip] = current_retry + 1
            
            if current_retry < MAX_RETRIES - 1:
                # Trigger ESP32 to capture again (bypass ESP8266)
                use_flash = current_retry >= 3
                trigger_esp32_capture(use_flash)
                print(f"🔄 GATE REMAINS CLOSED - Image unclear, retrying... (attempt {current_retry + 1})")
                return jsonify({
                    "error": "Image unclear, retrying...", 
                    "retry": current_retry + 1,
                    "flash": use_flash,
                    "gate_status": "Gate Remains Closed - Retrying Image Capture"
                })
            else:
                retry_count[client_ip] = 0
                print(f"🚫 GATE REMAINS CLOSED - Max retries reached, no clear plate detected")
                return jsonify({"error": "Max retries reached, image still unclear", "gate_status": "Gate Remains Closed - No Clear Plate Detected"})
        
        # Success - process the plate
        retry_count[client_ip] = 0
        plate_number = result_text
        print(f"✅ Plate detected: {plate_number}")
        
        if entry_type == 'entry':
            print("🚗 Processing ENTRY")
            return handle_entry(plate_number)
        else:
            print("🚗 Processing EXIT")
            return handle_exit(plate_number)
        
    except Exception as e:
        print(f"❌ ERROR in analyze_plate: {str(e)}")
        import traceback
        traceback.print_exc()
        return jsonify({"error": str(e)})

def handle_entry(plate_number):
    """Handle car entry logic"""
    try:
        # Check if car has booking
        booking = cars_collection.find_one({
            "plate": plate_number, 
            "exit_time": None
        })
        
        if booking and booking.get('entry_time'):
            print(f"🚫 GATE REMAINS CLOSED - Car {plate_number} already parked")
            return jsonify({"error": "Car already parked", "action": "deny", "gate_status": "Gate Remains Closed - Car Already Parked"})
        
        # Update booking with entry time or create new entry
        if booking:
            cars_collection.update_one(
                {"_id": booking['_id']},
                {"$set": {"entry_time": datetime.now()}}
            )
            booking_id = booking['booking_id']
        else:
            booking_id = f"BK{datetime.now().strftime('%Y%m%d%H%M%S')}"
            entry_record = {
                "plate": plate_number,
                "entry_time": datetime.now(),
                "exit_time": None,
                "slot": None,
                "booking_id": booking_id
            }
            cars_collection.insert_one(entry_record)
        
        # Trigger gate to open
        gate_success = trigger_esp8266_gate('open')
        if gate_success:
            print(f"🚪 GATE OPENING - Plate {plate_number} detected and verified")
        else:
            print(f"⚠️ GATE COMMAND FAILED - Plate {plate_number} detected but gate may not open")
        
        # Notify display controller about new entry
        try:
            url = f"http://{ESP8266_DISPLAY_IP}/set_plate"
            data = {"plate": plate_number}
            requests.post(url, json=data, timeout=5)
            print(f"📝 Plate sent to display controller: {plate_number}")
        except Exception as e:
            print(f"❌ Failed to notify display controller: {e}")
        
        return jsonify({
            "success": True,
            "plate_text": plate_number,
            "action": "allow",
            "booking_id": booking_id,
            "gate_status": "Gate Opening - Plate Detected"
        })
        
    except Exception as e:
        return jsonify({"error": str(e)})

def handle_exit(plate_number):
    """Handle car exit logic"""
    try:
        # Find the car record
        car_record = cars_collection.find_one({
            "plate": plate_number,
            "exit_time": None
        })
        
        if not car_record:
            print(f"🚫 GATE REMAINS CLOSED - Car {plate_number} not found or already exited")
            return jsonify({"error": "Car not found or already exited", "gate_status": "Gate Remains Closed - Car Not Found"})
        
        # Calculate bill
        bill_amount = calculate_bill(car_record['entry_time'])
        
        # Update exit time
        cars_collection.update_one(
            {"_id": car_record['_id']},
            {"$set": {"exit_time": datetime.now()}}
        )
        
        # Send bill to ESP8266 Display Controller for OLED display
        try:
            url = f"http://{ESP8266_DISPLAY_IP}/display_bill"
            data = {"plate": plate_number, "amount": bill_amount}
            requests.post(url, json=data, timeout=5)
            print(f"💰 Bill sent to display: {plate_number} - Rs.{bill_amount}")
        except Exception as e:
            print(f"❌ Display bill failed: {e}")
        
        # Open gate for exit
        gate_success = trigger_esp8266_gate('open')
        if gate_success:
            print(f"🚪 GATE OPENING - Exit approved for {plate_number}, Bill: Rs.{bill_amount}")
        else:
            print(f"⚠️ GATE COMMAND FAILED - Exit approved for {plate_number} but gate may not open")
        
        return jsonify({
            "success": True,
            "plate_text": plate_number,
            "bill_amount": bill_amount,
            "action": "exit",
            "gate_status": "Gate Opening - Exit Approved"
        })
        
    except Exception as e:
        return jsonify({"error": str(e)})


@app.route('/api/update_slot', methods=['POST'])
def update_slot():
    """Update parking slot for a car"""
    try:
        print(f"\n=== SLOT UPDATE REQUEST ===")
        print(f"Request from: {request.remote_addr}")
        data = request.get_json()
        plate = data.get('plate')
        slot = data.get('slot')
        print(f"Plate: {plate}, Slot: {slot}")
        
        result = cars_collection.update_one(
            {"plate": plate, "exit_time": None},
            {"$set": {"slot": slot}}
        )
        
        print(f"MongoDB update: matched={result.matched_count}, modified={result.modified_count}")
        
        if result.matched_count == 0:
            print(f"⚠️ No matching vehicle found for plate: {plate}")
        else:
            print(f"✅ Slot {slot} assigned to {plate}")
        
        return jsonify({"success": True})
    except Exception as e:
        print(f"❌ Slot update error: {e}")
        return jsonify({"error": str(e)})

@app.route('/status')
def status():
    return jsonify({
        "status": "ready", 
        "esp32_cam_ip": ESP32_CAM_IP,
        "esp8266_entry_ip": ESP8266_ENTRY_IP,
        "esp8266_display_ip": ESP8266_DISPLAY_IP
    })

@app.route('/api/system_status')
def system_status():
    """Check connectivity to all ESP devices"""
    devices = {
        "esp32_cam": {"ip": ESP32_CAM_IP, "status": "unknown"},
        "esp8266_entry": {"ip": ESP8266_ENTRY_IP, "status": "unknown"},
        "esp32_display": {"ip": ESP8266_DISPLAY_IP, "status": "unknown"}
    }
    
    # Test ESP32-CAM
    try:
        response = requests.get(f"http://{ESP32_CAM_IP}/status", timeout=3)
        devices["esp32_cam"]["status"] = "online" if response.status_code == 200 else "error"
    except:
        devices["esp32_cam"]["status"] = "offline"
    
    # Test ESP8266 Entry Controller
    try:
        response = requests.get(f"http://{ESP8266_ENTRY_IP}/status", timeout=3)
        devices["esp8266_entry"]["status"] = "online" if response.status_code == 200 else "error"
    except:
        devices["esp8266_entry"]["status"] = "offline"
    
    # Test ESP32 Display Controller
    try:
        response = requests.get(f"http://{ESP8266_DISPLAY_IP}/status", timeout=3)
        devices["esp32_display"]["status"] = "online" if response.status_code == 200 else "error"
    except:
        devices["esp32_display"]["status"] = "offline"
    
    return jsonify(devices)

@app.route('/<path:path>')
def serve_static(path):
    return send_from_directory('web', path)


if __name__ == "__main__":
    print("\n=== Smart Parking System Server ===")
    print(f"ESP32-CAM IP: {ESP32_CAM_IP}")
    print(f"ESP8266 Entry IP: {ESP8266_ENTRY_IP}")
    print(f"ESP32 Display IP: {ESP8266_DISPLAY_IP}")
    print("===================================\n")
    
    # Run without debug mode to prevent transformers reloading
    app.run(host='0.0.0.0', port=int(os.environ.get('PORT', 5000)), debug=False)
