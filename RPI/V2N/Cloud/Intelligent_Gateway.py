import paho.mqtt.client as mqtt
import ssl
import json
import time
import threading

broker = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
port = 8883
username = "v2n_admin"
password = "V2n@2026!"

zone = "zone1"

latest_state = None
car_counter = 0
ambulance_active = False

# Function to calculate green light duration based on number of cars
def calculate_green_time(count):
    if count < 5:
        return 8
    elif count < 15:
        return 12
    else:
        return 20

# Reset the car counter every 5 seconds
def reset_counter():
    global car_counter
    while True:
        time.sleep(5)
        print(f"Cars in last 5 sec: {car_counter}")
        car_counter = 0

# Called when the client successfully connects to the MQTT broker
def on_connect(client, userdata, flags, rc):
    print("Connected")
    
    # Subscribe to traffic light state coming from the ESP
    client.subscribe(f"v2n/traffic/light/state")
    
    # Subscribe to car ID messages
    client.subscribe(f"V2X/{zone}/car_id")

# Called whenever a message is received from the broker
def on_message(client, userdata, msg):
    global latest_state, car_counter, ambulance_active

    topic = msg.topic
    data = json.loads(msg.payload.decode())

    # Message coming from the ESP traffic light
    if topic == "v2n/traffic/light/state":
        latest_state = data
        process_and_publish()

    # Message coming from a vehicle
    elif topic == f"V2X/{zone}/car_id":
        vehicle_id = data["id"]

        # Check if the vehicle is an ambulance
        if vehicle_id == "AMB123":
            ambulance_active = True
            print("🚑 Ambulance detected!")

        else:
            # Increment car counter for normal vehicles
            car_counter += 1

# Process the received data and publish updated traffic information
def process_and_publish():
    global latest_state, car_counter, ambulance_active

    if latest_state is None:
        return

    # Copy the current traffic light state
    output = latest_state.copy()

    # Emergency priority: ambulance detected
    if ambulance_active:
        output["state"] = "GREEN"
        output["remaining_time"] = 20
        output["warning"] = "Ambulance Passing"
        ambulance_active = False

    else:
        # Adjust green time based on traffic density
        if output["state"] == "GREEN":
            new_time = calculate_green_time(car_counter)
            output["remaining_time"] = new_time

        output["density"] = car_counter
        output["warning"] = "Normal"

    # Publish the processed traffic state to vehicles
    client.publish(f"V2X/{zone}/traffic/processed", json.dumps(output))
    print("Published:", output)

# Create MQTT client
client = mqtt.Client()

# Set authentication credentials
client.username_pw_set(username, password)

# Enable TLS secure connection
client.tls_set(tls_version=ssl.PROTOCOL_TLS)

# Assign callback functions
client.on_connect = on_connect
client.on_message = on_message

# Connect to the broker
client.connect(broker, port)

# Start a background thread to reset the car counter periodically
threading.Thread(target=reset_counter, daemon=True).start()

# Keep the client running and listening for messages
client.loop_forever()