import serial
import time

try:
    print("Opening COM12 at 115200 baud...")
    ser = serial.Serial('COM12', 115200, timeout=1)
    print("Port opened successfully")
    
    # Send GET command
    print("Sending GET command...")
    ser.write(b'GET\n')
    
    # Wait for response
    time.sleep(0.5)
    
    # Check if data is available
    bytes_waiting = ser.in_waiting
    print(f'Bytes waiting: {bytes_waiting}')
    
    if bytes_waiting > 0:
        response = ser.readline().decode().strip()
        print(f'Response: "{response}"')
    else:
        print("No response received")
    
    # Try sending another command
    print("\nSending SET HEATER command...")
    ser.write(b'SET HEATER\n')
    
    # Wait for response
    time.sleep(0.5)
    
    # Check if data is available
    bytes_waiting = ser.in_waiting
    print(f'Bytes waiting: {bytes_waiting}')
    
    if bytes_waiting > 0:
        response = ser.readline().decode().strip()
        print(f'Response: "{response}"')
    else:
        print("No response received")
    
    # Close the port
    ser.close()
    print("Port closed")
    
except Exception as e:
    print(f"Error: {e}")