import requests
import os

def test_server():
    """Test the number plate recognition server with local image"""
    server_url = "http://localhost:5000"
    image_file = "random.webp"
    
    print("Testing Number Plate Recognition Server...")
    print("-" * 40)
    
    # Check if image file exists
    if not os.path.exists(image_file):
        print(f"✗ Image file '{image_file}' not found")
        return
    
    # Test server connectivity
    try:
        response = requests.get(server_url, timeout=5)
        print("✓ Server is running")
    except requests.exceptions.RequestException as e:
        print(f"✗ Server not accessible: {e}")
        return
    
    # Test with local image
    print(f"\nTesting with local image: {image_file}")
    
    with open(image_file, 'rb') as f:
        files = {'image': (image_file, f, 'image/jpeg')}
        
        try:
            response = requests.post(f"{server_url}/api/analyze-plate", files=files, timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                if 'error' in result:
                    print(f"✗ API Error: {result['error']}")
                else:
                    print(f"✓ Success! Detected: {result['plate_text']}")
            else:
                print(f"✗ HTTP Error: {response.status_code}")
                print(f"Response: {response.text}")
                
        except requests.exceptions.RequestException as e:
            print(f"✗ Request failed: {e}")

if __name__ == "__main__":
    test_server()