# Story 006: UART Protocol with Matter Controller

## The Problem

The Death Fortune Teller needs a reliable communication protocol between the main ESP32-WROVER controller and a separate Matter controller. The Matter controller handles proximity detection and state management, while the main controller handles all the physical effects (audio, servo, LEDs, thermal printing).

## The Solution

A dual-protocol UART communication system at 115200 baud:

### Text-Based Commands
Simple ASCII commands for basic triggers:
- `NEAR\n` - Person detected near skull
- `FAR\n` - Person walking away  
- `[STATE_NAME]\n` - Trigger specific state manually

### Binary Protocol with Handshakes
Reliable communication with acknowledgment system:

**Commands from Matter Controller:**
- `CMD_BOOT_HELLO` (0x0D) - Sent immediately after boot, every 5 seconds until acknowledged
- `CMD_FABRIC_HELLO` (0x0E) - Sent every 5 seconds after Matter commissioning until acknowledged

**Responses from Death Controller:**
- `RSP_BOOT_ACK` (0x90) - Acknowledges boot handshake
- `RSP_FABRIC_ACK` (0x91) - Acknowledges Matter fabric handshake

### Frame Format
```
[START_BYTE] [LENGTH] [COMMAND] [PAYLOAD] [CRC]
    0xA5       1-255   1 byte   0-253     1 byte
```

## Implementation Details

The UART controller now handles:
1. **Text Protocol**: Parses ASCII commands and converts to internal state triggers
2. **Binary Protocol**: Maintains existing frame-based command system
3. **Handshake Management**: Tracks boot and fabric connection status
4. **Dual Parsing**: Attempts both text and binary parsing for each received message

## Benefits

- **Reliability**: Handshake system ensures both controllers know connection status
- **Flexibility**: Both text and binary protocols supported
- **Debugging**: Clear separation between proximity detection and state management
- **Robustness**: Graceful handling of mixed protocol messages

## Usage

The Matter controller can now:
- Send simple text commands for basic triggers
- Maintain reliable connection status with handshakes
- Trigger any state in Death's state machine
- Get immediate feedback on connection health

This enables a clean separation of concerns where the Matter controller focuses on proximity and network management while Death handles all the physical effects.
