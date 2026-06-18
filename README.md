# Aqua:eq8

> Open Hardware. Open Source. Smart Irrigation.

Aqua:eq8 is an open-source and open-hardware 8-zone smart irrigation controller designed for residential, commercial, greenhouse, and agricultural installations.

Built around the ESP32-C3-MINI-N4, Aqua:eq8 combines reliable irrigation control with secure connectivity, mobile management, OTA firmware updates, MQTT communication, and cloud integration while remaining fully self-hostable and hackable.

The project includes:

- ESP32 firmware
- Hardware schematics and PCB design
- Golang backend API
- React Native mobile application
- Docker, Kubernetes, and Helm deployment support

---

## Features

### Irrigation Control

- 8 independent irrigation zones
- Multiple zones can run simultaneously
- Configurable Master Valve support
- Configurable pump start delay
- Local manual operation without Internet
- Mobile application control
- MQTT-based communication
- Future scheduling support
- Future weather integration
- Emergency stop functionality

### Connectivity

- WiFi provisioning via BLE
- Bluetooth Low Energy onboarding
- MQTT communication
- OTA firmware updates
- Cloud synchronization
- Device telemetry

### Security

- ESP32 Secure Boot
- Flash Encryption
- Signed OTA updates
- TLS communication
- Device authentication
- Secure credential storage

### Reliability

- Offline-first operation
- Local control during Internet outages
- Hardware watchdog
- Software watchdog
- Automatic recovery
- Power-loss recovery

### Open Source

- Open firmware
- Open hardware
- Open API
- Open mobile app
- Self-hostable backend

---

# Repository Structure

```
aqua-eq8/
├── firmware/
│   ├── main/
│   ├── components/
│   ├── ble/
│   ├── wifi/
│   ├── mqtt/
│   ├── ota/
│   └── valve/
│
├── hardware/
│   ├── schematic/
│   ├── pcb/
│   ├── enclosure/
│   └── bom/
│
├── api/
│   ├── cmd/
│   ├── internal/
│   ├── deployments/
│   ├── helm/
│   └── docker/
│
├── mobile/
│   ├── src/
│   ├── ios/
│   └── android/
│
└── docs/

```

# Hardware

## Power

The controller is powered by a standard irrigation transformer.

### Input

text 24VAC Barrel Jack 

### Internal Rails

```
text 24VAC  ├── Valve Outputs  ├── Current Monitoring  └── AC/DC Converter       ├── 5V       └── 3.3V 
```
---

## Main Components

### MCU

- ESP32-C3-MINI-N4
- WiFi 2.4GHz
- Bluetooth Low Energy
- 4MB Flash

### Output Expansion

Two 74HC595 shift registers are used:

#### Shift Register #1

Controls:

- 8 TRIAC outputs
- 8 Green status LEDs

#### Shift Register #2

Controls:

- 8 Red Master LEDs

This minimizes GPIO usage while allowing future expansion.

### Valve Drivers

- TRIAC-based switching
- Designed for 24VAC sprinkler valves
- RC snubber protection per channel
- Future current sensing support

### Protection

- Input fuse
- Snubber protection on every channel
- Brownout protection
- Hardware watchdog
- Secure boot
- Flash encryption

---

# Channel Controls

## Channels 1-8

Each button controls one irrigation zone.

### Short Press

Toggle zone state.

| Current State | Action |
|--------------|----------|
| OFF | Turn ON |
| ON | Turn OFF |

### Long Press

Toggle Master Valve assignment.

| Current State | Action |
|--------------|----------|
| Normal Zone | Set as Master |
| Master Zone | Remove Master |

Only one channel may be configured as Master at a time.

---

# Master Valve Operation

A Master Valve is commonly used for:

- Main water supply
- Pump control
- Pressure system enable

When any non-master zone is enabled:

1. Master Valve turns ON immediately
2. Zone enters waiting state
3. Configurable delay timer starts
4. Zone activates after delay

Example:

text Channel 1 = Master  User enables Channel 3  Channel 1 ON immediately  Channel 3 LED flashes  5 second delay  Channel 3 ON 

When the last active zone stops:

text Channel 3 OFF  No active zones remaining  Channel 1 OFF 

### Default Delay

yaml master_delay_seconds: 5 

Configurable through the mobile application.

---

# Channel LED Behavior

Each channel button contains:

- Green LED
- Red LED

## Green LED

Indicates operational state.

| State | Green |
|---------|---------|
| OFF | OFF |
| Waiting Master Delay | Flashing |
| Active | Solid |

## Red LED

Indicates configuration state.

| State | Red |
|---------|---------|
| Normal Zone | OFF |
| Master Zone | Solid |

## Combined States

| State | Green | Red |
|---------|---------|---------|
| OFF | OFF | OFF |
| ON | ON | OFF |
| Master OFF | OFF | ON |
| Master ON | ON | ON |
| Waiting Delay | Flashing | OFF |

---

# Status Button (Button 9)

Button 9 is a dedicated RGB status and management button.

Functions:

- Device Status
- BLE Pairing
- Factory Reset

---

## RGB Status LED

The status LED always displays the highest priority state.

Priority:

text Error OTA Update BLE Connected BLE Advertising WiFi Connected Booting 

### States

| State | RGB |
|---------|---------|
| Booting | White Breathing |
| WiFi Connected | Solid Green |
| BLE Advertising | Blue Breathing |
| Mobile App Connected | Solid Blue |
| OTA Update | Purple Breathing |
| Error | Red Flashing |

---

## Button Actions

### Short Press

Enable BLE pairing mode.

LED:

text Blue Breathing 

Duration:

text 10 Minutes 

### Long Press (10 Seconds)

Factory Reset.

Deletes:

- WiFi Credentials
- MQTT Configuration
- Device Pairing
- Local Configuration
- Schedules

LED:

text Red Breathing 

Device automatically reboots.

---

# Connectivity

Aqua:eq8 intentionally avoids:

- Captive Portals
- Access Point Setup
- Embedded Web UI

All device provisioning is performed through Bluetooth Low Energy.

### Provisioning Flow

text Power Device       ↓ Press Status Button       ↓ BLE Pairing       ↓ Mobile App       ↓ Send WiFi Credentials       ↓ Connect to Network       ↓ Register with Backend       ↓ MQTT Connected 

---

# MQTT

MQTT is the primary communication mechanism between the controller and backend services.

## Topics

text aquaeq8/{deviceId}/state aquaeq8/{deviceId}/events aquaeq8/{deviceId}/telemetry aquaeq8/{deviceId}/command aquaeq8/{deviceId}/ota 

## Example Telemetry

json {   "uptime": 86400,   "wifi_rssi": -62,   "free_heap": 124832,   "active_zones": [2,4,7] } 

MQTT data can be consumed directly by:

- Telegraf
- InfluxDB
- TimescaleDB
- VictoriaMetrics
- Grafana

---

# Firmware

Location:

text /firmware 

Built using:

- ESP-IDF
- FreeRTOS
- C/C++

## Features

### Device Control

- Zone management
- Master Valve logic
- Delay handling
- Local operation

### Connectivity

- BLE provisioning
- WiFi management
- MQTT client

### OTA

- Signed firmware updates
- Rollback protection
- Secure update process

### Security

- Secure Boot
- Flash Encryption
- Device authentication

### Reliability

- Hardware watchdog
- Software watchdog
- Crash recovery

---

# Backend API

Location:

text /api 

Built using:

- Go 1.26+
- PostgreSQL
- Redis
- MQTT

## Features

### Device Management

- Device registration
- Pairing
- Configuration synchronization

### Monitoring

- Online/offline status
- Telemetry collection
- Device diagnostics

### Security

- JWT authentication
- TLS communication
- API rate limiting

---

# Deployment

## Docker

bash docker compose up -d 

## Kubernetes

bash kubectl apply -f deployments/ 

## Helm

bash helm install aquaeq8 ./helm 

---

# Mobile Application

Location:

text /mobile 

Built using:

- React Native
- TypeScript

## Features

### Device Setup

- BLE discovery
- WiFi provisioning
- Device pairing

### Device Management

- Zone control
- Master Valve configuration
- Delay configuration

### Monitoring

- Device health
- Connectivity status
- Telemetry visualization

### OTA

- Firmware update notifications
- Update progress monitoring

---

# Future Features

## Planned

- Irrigation schedules
- Weather integration
- Rain delay
- Seasonal adjustments
- Water usage analytics

## Hardware Expansion

- Flow meter support
- Rain sensor input
- Valve current monitoring
- Leak detection
- Pump monitoring

---

# Valve Current Monitoring

Future versions may include electrical current sensing on each output channel.

Benefits:

- Detect disconnected valves
- Detect broken wires
- Detect short circuits
- Detect stuck valves
- Improve diagnostics

Example:

text Zone 4 ON  Expected Current: 220mA Measured Current: 0mA  Alert: Valve Not Connected 

---

# Development

## [Firmware](firmware)


bash cd firmware  idf.py build idf.py flash idf.py monitor 

## [Backend](api)

bash cd api  go run ./cmd/server 

## [Mobile](mobile)

bash cd mobile  yarn install

bash cd mobile  yarn install yarn start 

---

# Security Model

Every controller includes:

- Unique Device ID
- Device Certificate
- Private Key

Firmware security:

- Secure Boot enabled
- Flash Encryption enabled
- Signed OTA updates only

The controller never accepts unsigned firmware images.

---

# Contributing

Contributions are welcome.

Areas where help is needed:

- Embedded development
- Hardware design
- Backend development
- Mobile development
- Testing
- Documentation

Please open an issue or submit a pull request.

---

# License

MIT License

See [LICENSE](LICENSE) for details.

---

# Philosophy

Aqua:eq8 is built around three principles.

## Open Source

All software is publicly available.

## Open Hardware

PCB layouts and schematics are publicly available.

## Local First

Your irrigation system continues operating even if the Internet does not.

---

Aqua:eq8

An open-source, secure, MQTT-native irrigation controller built for modern gardens, farms, and greenhouses.