# Arduino CLI ESP32 Workflow

This guide shows how to compile, upload, and monitor ESP32 sketches using `arduino-cli`.

## 1. List Connected Boards

Use the following command to detect all connected boards and their ports:

```
arduino-cli board list
```

Look for ports such as:

```
/dev/ttyACM0
/dev/ttyACM1
```

## 2. Compile the Sketch

Run the following command inside the sketch directory:

```
arduino-cli compile --fqbn esp32:esp32:esp32s3
```

This compiles the sketch for the **ESP32-S3 Dev Module**.

## 3. Upload the Firmware

Upload the compiled sketch to the ESP32:

```
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3
```

Replace `/dev/ttyACM0` with the correct port from the board list.

## 4. Open the Serial Monitor

To view serial output from the board:

```
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Ensure the baud rate matches the value used in the sketch (e.g. `Serial.begin(115200)`).

## Example Workflow

```
arduino-cli board list
arduino-cli compile --fqbn esp32:esp32:esp32s3
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```