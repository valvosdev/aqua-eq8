#!/bin/bash

# Check if both parameters are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <deviceId> <scheduleNumber>"
    exit 1
fi

DEVICE_ID=$1
SCHEDULE_NUM=$2

# Validate schedule number is between 1 and 4
if ! [[ "$SCHEDULE_NUM" =~ ^[1-4]$ ]]; then
    echo "Error: Schedule number must be between 1 and 4."
    exit 1
fi

# Execute the MQTT publish command to stop
mosquitto_pub -h 10.30.10.30 -p 8883 -t "tenant/0001000/device/${DEVICE_ID}/schedule/${SCHEDULE_NUM}/stop" -m "" \
  --cafile ./certs/valvos-dev-ca.crt --cert ./certs/client.crt --key ./certs/client.key --insecure

