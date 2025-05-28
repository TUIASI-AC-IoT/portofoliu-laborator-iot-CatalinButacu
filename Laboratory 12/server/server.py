import os
from threading import Thread, Lock
from serial import Serial
from flask import Flask, jsonify, send_from_directory, request
from flask_swagger_ui import get_swaggerui_blueprint
from time import sleep
import serial.tools.list_ports
import atexit
import random
import datetime

# List available serial ports
print("Available serial ports:")
ports = list(serial.tools.list_ports.comports())
for port in ports:
    print(f"  {port.device}: {port.description}")

# Serial port configuration
PORT = 'COM12'
BAUDRATE = 115200
serial_port = None
simulation_mode = False

try:
    print(f"Attempting to connect to {PORT} at {BAUDRATE} baud...")
    serial_port = Serial(PORT, BAUDRATE, timeout=1)
    print(f"Connected to {PORT} at {BAUDRATE} baud")
    # Test the connection
    serial_port.write(b'\n')  # Send a newline to clear any pending input
    sleep(0.5)
    if serial_port.in_waiting:
        print(f"Data available in buffer: {serial_port.in_waiting} bytes")
        test_read = serial_port.readline().decode('utf-8').strip()
        print(f"Test read: '{test_read}'")
    else:
        print("No data in buffer after connection test")
        print("WARNING: ESP32 is not responding. Make sure it's properly connected and programmed.")
        print("Continuing in limited functionality mode...")
        
    # Register function to close serial port on exit
    def close_serial():
        if serial_port and serial_port.is_open:
            print("Closing serial port...")
            serial_port.close()
    
    atexit.register(close_serial)
    
except Exception as e:
    print(f"Error connecting to serial port: {e}")
    print("Running in simulation mode without actual device")
    simulation_mode = True

mutex = Lock()
last_temp = 20.0  # Start with a reasonable default temperature for the simulation
last_update_time = datetime.datetime.now()

app = Flask(__name__)

# Configure Swagger UI
SWAGGER_URL = '/swagger'
API_URL = '/static/swagger.json'

# Create Swagger UI blueprint
swagger_ui_blueprint = get_swaggerui_blueprint(
    SWAGGER_URL,
    API_URL,
    config={
        'app_name': "IoT Temperature Sensor API"
    }
)

# Register blueprint
app.register_blueprint(swagger_ui_blueprint, url_prefix=SWAGGER_URL)

# Serve swagger.json
@app.route('/static/swagger.json')
def serve_swagger_spec():
    return send_from_directory(os.path.dirname(os.path.abspath(__file__)), 'swagger.json')

# Function to generate simulated temperature data
def generate_simulated_temp():
    global last_temp
    # Add some random variation to the temperature
    variation = random.uniform(-0.5, 0.5)
    last_temp = max(10, min(30, last_temp + variation))  # Keep between 10-30°C
    return last_temp

# Thread to continuously read temperature data from ESP32
def read_sensor_data():
    global last_temp, serial_port, last_update_time, simulation_mode
    consecutive_failures = 0
    max_failures = 10  # Switch to simulation mode after this many consecutive failures
    
    while True:
        try:
            if simulation_mode:
                # Generate simulated temperature data
                temp = generate_simulated_temp()
                mutex.acquire()
                last_temp = temp
                last_update_time = datetime.datetime.now()
                mutex.release()
                print(f"Simulated temperature: {temp:.2f}°C")
            elif serial_port and serial_port.is_open:
                # Send GET command to ESP32
                print(f"Sending GET command to ESP32")
                serial_port.write(b'GET\n')
                sleep(0.7)
                
                # Check if data is available
                if serial_port.in_waiting > 0:
                    # Read response
                    response = serial_port.readline().decode('utf-8').strip()
                    print(f"Raw response from ESP32: '{response}'")
                    if response:
                        try:
                            # Try to extract a float from the response
                            # This handles cases where the ESP32 might output debug info
                            import re
                            float_matches = re.findall(r'\d+\.\d+', response)
                            if float_matches:
                                temp = float(float_matches[0])
                                mutex.acquire()
                                last_temp = temp
                                last_update_time = datetime.datetime.now()
                                mutex.release()
                                print(f"Temperature updated to: {temp}")
                                consecutive_failures = 0  # Reset failure counter on success
                            else:
                                print(f"No temperature value found in response: '{response}'")
                                consecutive_failures += 1
                        except ValueError as e:
                            print(f"Invalid temperature value: '{response}', error: {e}")
                            consecutive_failures += 1
                    else:
                        print("Empty response from ESP32")
                        consecutive_failures += 1
                else:
                    print("No data received from ESP32 after GET command")
                    consecutive_failures += 1
                    
                # Check if we should switch to simulation mode
                if consecutive_failures >= max_failures and not simulation_mode:
                    print(f"WARNING: {consecutive_failures} consecutive failures communicating with ESP32")
                    print("Switching to simulation mode")
                    simulation_mode = True
            else:
                # Try to reconnect if serial port is closed
                try:
                    if serial_port and not serial_port.is_open:
                        print("Attempting to reopen serial port...")
                        serial_port.open()
                        consecutive_failures = 0
                    elif not serial_port:
                        print("No serial port available, switching to simulation mode")
                        simulation_mode = True
                except Exception as reconnect_error:
                    print(f"Failed to reopen serial port: {reconnect_error}")
                    consecutive_failures += 1
                    
                # Check if we should switch to simulation mode
                if consecutive_failures >= max_failures and not simulation_mode:
                    print(f"WARNING: {consecutive_failures} consecutive failures communicating with ESP32")
                    print("Switching to simulation mode")
                    simulation_mode = True
        except Exception as e:
            print(f"Error in sensor reading thread: {e}")
            consecutive_failures += 1
            
            # Check if we should switch to simulation mode
            if consecutive_failures >= max_failures and not simulation_mode:
                print(f"WARNING: {consecutive_failures} consecutive failures communicating with ESP32")
                print("Switching to simulation mode")
                simulation_mode = True
        
        sleep(2)  # Read temperature every 2 seconds

# Start the sensor reading thread
sensor_thread = Thread(target=read_sensor_data, daemon=True)
sensor_thread.start()
print("Sensor reading thread started")

# Function to send commands to the ESP32
def send_command(command):
    if not simulation_mode and serial_port and serial_port.is_open:
        try:
            print(f"[DEBUG] Attempting to send command to ESP32: '{command}'")
            # Clear any pending data in the buffer
            if serial_port.in_waiting > 0:
                serial_port.reset_input_buffer()
                
            serial_port.write(f"{command}\n".encode('utf-8'))
            sleep(0.3)  # Give ESP32 more time to respond
            
            # Check if data is available
            if serial_port.in_waiting > 0:
                response = serial_port.readline().decode('utf-8').strip()
                print(f"[DEBUG] Received raw response from ESP32: '{response}'")
                
                # If we get a response with debug info, try to extract the actual response
                if "OK" in response:
                    return "OK"
                elif "ERROR" in response:
                    return "ERROR"
                else:
                    return response
            else:
                print("No response received from ESP32")
                return "No response"
        except Exception as e:
            print(f"Error sending command: {e}")
            return f"Error: {e}"
    return "Simulation mode: Command would be sent"

# Get temperature reading
@app.route('/temperature', methods=['GET'])
def get_temperature():
    mutex.acquire()
    value = last_temp
    update_time = last_update_time
    mutex.release()
    
    # Calculate how old the data is
    age_seconds = (datetime.datetime.now() - update_time).total_seconds()
    
    return jsonify({
        "value": round(value, 2),
        "unit": "°C",
        "timestamp": update_time.isoformat(),
        "age_seconds": age_seconds,
        "simulation_mode": simulation_mode
    })

# Control endpoints for heater and cooler
@app.route('/control/<action>', methods=['POST'])
def control_device(action):
    action = action.upper()
    if action == "HEATER":
        # Try both command formats
        response = send_command("HEATER")
        if response == "No response" or response.startswith("Error"):
            # If the first command fails, try the alternative format
            response = send_command("SET HEATER")
        print(f"[INFO] Heater action '{action}' initiated. Response: {response}")
        return jsonify({"message": "Heater activated", "response": response, "simulation_mode": simulation_mode})
    elif action == "COOLER":
        # Try both command formats
        response = send_command("COOLER")
        if response == "No response" or response.startswith("Error"):
            # If the first command fails, try the alternative format
            response = send_command("SET COOLER")
        print(f"[INFO] Cooler action '{action}' initiated. Response: {response}")
        return jsonify({"message": "Cooler activated", "response": response, "simulation_mode": simulation_mode})
    elif action == "STOP":
        response = send_command("STOP")
        print(f"[INFO] Stop action '{action}' initiated. Response: {response}")
        return jsonify({"message": "Device stopped", "response": response, "simulation_mode": simulation_mode})
    else:
        return jsonify({"error": "Invalid action. Use 'heater', 'cooler', or 'stop'"}), 400

# New endpoint to manually send commands to ESP32 when PlatformIO monitor is using the port
@app.route('/manual_command', methods=['POST'])
def manual_command():
    data = request.get_json()
    if not data or 'command' not in data:
        return jsonify({"error": "Missing 'command' parameter"}), 400
    
    command = data['command']
    print(f"Manual command requested: '{command}'")
    
    # If we're in simulation mode, provide instructions for the user
    if simulation_mode:
        return jsonify({
            "message": "Server is in simulation mode",
            "instructions": "To execute this command, type it manually in the PlatformIO monitor: " + command,
            "simulation_mode": True
        })
    else:
        response = send_command(command)
        return jsonify({
            "message": "Command sent",
            "command": command,
            "response": response,
            "simulation_mode": False
        })

# Endpoint to check serial port status
@app.route('/status', methods=['GET'])
def get_status():
    if serial_port:
        try:
            is_open = serial_port.is_open
            in_waiting = serial_port.in_waiting if is_open else 0
            return jsonify({
                "connected": is_open,
                "port": PORT,
                "baudrate": BAUDRATE,
                "bytes_waiting": in_waiting,
                "simulation_mode": simulation_mode,
                "last_temperature": round(last_temp, 2),
                "last_update": last_update_time.isoformat()
            })
        except Exception as e:
            return jsonify({
                "connected": False,
                "error": str(e),
                "simulation_mode": simulation_mode,
                "last_temperature": round(last_temp, 2),
                "last_update": last_update_time.isoformat()
            })
    else:
        return jsonify({
            "connected": False,
            "simulation_mode": simulation_mode,
            "last_temperature": round(last_temp, 2),
            "last_update": last_update_time.isoformat()
        })

# Endpoint to force simulation mode on/off
@app.route('/simulation', methods=['POST'])
def set_simulation_mode():
    global simulation_mode
    data = request.get_json()
    if not data or 'enabled' not in data:
        return jsonify({"error": "Missing 'enabled' parameter"}), 400
    
    simulation_mode = bool(data['enabled'])
    print(f"Simulation mode {'enabled' if simulation_mode else 'disabled'}")
    
    return jsonify({
        "message": f"Simulation mode {'enabled' if simulation_mode else 'disabled'}",
        "simulation_mode": simulation_mode
    })

if __name__ == '__main__':
    # Run without debug mode to prevent Flask from restarting and trying to reopen the serial port
    app.run(debug=False, host='0.0.0.0', port=5000)