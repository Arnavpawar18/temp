from pymongo import MongoClient
from datetime import datetime

# MongoDB connection
MONGO_URI = "mongodb+srv://pawararnav232:pawararnav1234@parkingsystem.nxycox7.mongodb.net/?retryWrites=true&w=majority&appName=parkingSystem"

try:
    print("Connecting to MongoDB...")
    client = MongoClient(MONGO_URI, serverSelectionTimeoutMS=5000, tls=True, tlsAllowInvalidCertificates=True)
    
    # Test connection
    client.admin.command('ping')
    print("SUCCESS: MongoDB connection successful!")
    
    # Access database
    db = client.parking_system
    cars_collection = db.cars
    
    # Check collections
    print(f"\nDatabase: parking_system")
    print(f"Collections: {db.list_collection_names()}")
    
    # Count documents
    total_cars = cars_collection.count_documents({})
    print(f"\nTotal vehicles in database: {total_cars}")
    
    # Currently parked
    parked = cars_collection.count_documents({"exit_time": None})
    print(f"Currently parked: {parked}")
    
    # Show recent entries
    print(f"\nRecent 5 entries:")
    recent = list(cars_collection.find().sort("_id", -1).limit(5))
    for car in recent:
        print(f"  - Plate: {car.get('plate', 'N/A')}, Entry: {car.get('entry_time', 'N/A')}, Exit: {car.get('exit_time', 'N/A')}")
    
    print("\nSUCCESS: Database is properly connected and working!")
    
except Exception as e:
    print(f"ERROR: Database connection failed: {e}")
