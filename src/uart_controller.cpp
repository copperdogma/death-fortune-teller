#include "uart_controller.h"
#include "logging_manager.h"
#include <cstring>

static constexpr const char* TAG = "UART";

UARTController::UARTController(int rxPin, int txPin)
    : lastCommand(UARTCommand::NONE),
      lastCommandTime(0),
      rxPin(rxPin),
      txPin(txPin),
      bootHandshakeComplete(false),
      fabricHandshakeComplete(false),
      lastBootHelloTime(0),
      lastFabricHelloTime(0) {
}

void UARTController::begin() {
    Serial1.begin(UART_BAUD, SERIAL_8N1, rxPin, txPin);
    Serial1.setRxBufferSize(RX_BUFFER_SIZE);
    LOG_INFO(TAG, "UART controller initialized (rx=%d tx=%d baud=%d)", rxPin, txPin, UART_BAUD);
}

void UARTController::update() {
    int availableBytes = Serial1.available();
    if (availableBytes > 0) {
        uint8_t buffer[RX_BUFFER_SIZE];
        size_t toRead = min(static_cast<size_t>(availableBytes), static_cast<size_t>(RX_BUFFER_SIZE));
        size_t length = Serial1.readBytes(buffer, toRead);
        
        // Output every single message received from Matter UART
        Serial.print("[UART RX] ");
        for (size_t i = 0; i < length; ++i) {
            Serial.printf("%02X ", buffer[i]);
        }
        Serial.printf("(len=%u)\n", static_cast<unsigned>(length));
        
        // Try parsing as text command first
        bool parsed = false;
        if (length > 0 && buffer[length - 1] == '\n') {
            // Null-terminate for string operations
            char textBuffer[RX_BUFFER_SIZE + 1];
            size_t textLen = min(length - 1, static_cast<size_t>(RX_BUFFER_SIZE)); // Exclude newline
            memcpy(textBuffer, buffer, textLen);
            textBuffer[textLen] = '\0';
            
            if (parseTextCommand(textBuffer, textLen)) {
                lastCommandTime = millis();
                parsed = true;
            }
        }
        
        // If text parsing failed, try binary frame parsing
        if (!parsed && parseFrame(buffer, length)) {
            lastCommandTime = millis();
            parsed = true;
        }
        
        if (!parsed && length > 0) {
            char sample[64] = {0};
            size_t sampleLen = min(length, static_cast<size_t>(8));
            for (size_t i = 0; i < sampleLen; ++i) {
                snprintf(sample + (i * 3), sizeof(sample) - (i * 3), "%02X ", buffer[i]);
            }
            LOG_WARN(TAG, "UART bytes read=%u but no valid command parsed (sample: %s)", static_cast<unsigned>(length), sample);
        }
    }
    
    // Clear command after timeout
    if (lastCommand != UARTCommand::NONE && millis() - lastCommandTime > COMMAND_TIMEOUT_MS) {
        lastCommand = UARTCommand::NONE;
    }
}

UARTCommand UARTController::getLastCommand() {
    return lastCommand;
}

void UARTController::clearLastCommand() {
    lastCommand = UARTCommand::NONE;
}

bool UARTController::parseFrame(uint8_t* buffer, size_t length) {
    if (length < 4) {
        LOG_WARN(TAG, "UART frame too short (len=%u)", static_cast<unsigned>(length));
        return false;
    }
    
    // Look for start byte
    bool startFound = false;
    for (size_t i = 0; i < length - 3; i++) {
        if (buffer[i] == FRAME_START) {
            startFound = true;
            uint8_t len = buffer[i + 1];
            size_t frameSize = static_cast<size_t>(len) + 3; // start + len byte + payload + CRC
            if (i + frameSize <= length) {
                uint8_t cmd = buffer[i + 2];
                uint8_t crc = buffer[i + len + 2];
                
                // Verify CRC
                uint8_t calculatedCRC = calculateCRC8(&buffer[i + 1], len + 1);
                if (calculatedCRC == crc) {
                    UARTCommand decodedCommand = commandFromByte(cmd);
                    if (decodedCommand == UARTCommand::NONE) {
                        LOG_WARN(TAG, "UART command unknown: 0x%02X", cmd);
                    } else {
                        LOG_INFO(TAG, "UART command received: %s (0x%02X)", commandToString(decodedCommand), cmd);
                        
                        // Handle handshake commands
                        if (decodedCommand == UARTCommand::BOOT_HELLO) {
                            lastBootHelloTime = millis();
                            sendBootAck();
                        } else if (decodedCommand == UARTCommand::FABRIC_HELLO) {
                            lastFabricHelloTime = millis();
                            sendFabricAck();
                        }
                    }
                    lastCommand = decodedCommand;
                    return true;
                } else {
                    LOG_WARN(TAG, "UART CRC mismatch: received 0x%02X calculated 0x%02X", crc, calculatedCRC);
                }
            } else {
                LOG_WARN(TAG, "UART frame incomplete: len byte=%u but only %u bytes buffered", len, static_cast<unsigned>(length - i));
            }
        }
    }
    if (!startFound) {
        LOG_WARN(TAG, "UART start byte not found in buffer (len=%u)", static_cast<unsigned>(length));
    }
    return false;
}

uint8_t UARTController::calculateCRC8(uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

UARTCommand UARTController::commandFromByte(uint8_t cmd) {
    switch (cmd) {
        case CMD_FAR_MOTION_TRIGGER: return UARTCommand::FAR_MOTION_TRIGGER;
        case CMD_NEAR_MOTION_TRIGGER: return UARTCommand::NEAR_MOTION_TRIGGER;
        case CMD_PLAY_WELCOME: return UARTCommand::PLAY_WELCOME;
        case CMD_WAIT_FOR_NEAR: return UARTCommand::WAIT_FOR_NEAR;
        case CMD_PLAY_FINGER_PROMPT: return UARTCommand::PLAY_FINGER_PROMPT;
        case CMD_MOUTH_OPEN_WAIT_FINGER: return UARTCommand::MOUTH_OPEN_WAIT_FINGER;
        case CMD_FINGER_DETECTED: return UARTCommand::FINGER_DETECTED;
        case CMD_SNAP_WITH_FINGER: return UARTCommand::SNAP_WITH_FINGER;
        case CMD_SNAP_NO_FINGER: return UARTCommand::SNAP_NO_FINGER;
        case CMD_FORTUNE_FLOW: return UARTCommand::FORTUNE_FLOW;
        case CMD_FORTUNE_DONE: return UARTCommand::FORTUNE_DONE;
        case CMD_COOLDOWN: return UARTCommand::COOLDOWN;
        case CMD_BOOT_HELLO: return UARTCommand::BOOT_HELLO;
        case CMD_FABRIC_HELLO: return UARTCommand::FABRIC_HELLO;
        case CMD_LEGACY_SET_MODE: return UARTCommand::LEGACY_SET_MODE;
        case CMD_LEGACY_PING: return UARTCommand::LEGACY_PING;
        default: return UARTCommand::NONE;
    }
}

const char* UARTController::commandToString(UARTCommand command) {
    switch (command) {
        case UARTCommand::FAR_MOTION_TRIGGER: return "FAR_MOTION_TRIGGER";
        case UARTCommand::NEAR_MOTION_TRIGGER: return "NEAR_MOTION_TRIGGER";
        case UARTCommand::PLAY_WELCOME: return "PLAY_WELCOME";
        case UARTCommand::WAIT_FOR_NEAR: return "WAIT_FOR_NEAR";
        case UARTCommand::PLAY_FINGER_PROMPT: return "PLAY_FINGER_PROMPT";
        case UARTCommand::MOUTH_OPEN_WAIT_FINGER: return "MOUTH_OPEN_WAIT_FINGER";
        case UARTCommand::FINGER_DETECTED: return "FINGER_DETECTED";
        case UARTCommand::SNAP_WITH_FINGER: return "SNAP_WITH_FINGER";
        case UARTCommand::SNAP_NO_FINGER: return "SNAP_NO_FINGER";
        case UARTCommand::FORTUNE_FLOW: return "FORTUNE_FLOW";
        case UARTCommand::FORTUNE_DONE: return "FORTUNE_DONE";
        case UARTCommand::COOLDOWN: return "COOLDOWN";
        case UARTCommand::BOOT_HELLO: return "BOOT_HELLO";
        case UARTCommand::FABRIC_HELLO: return "FABRIC_HELLO";
        case UARTCommand::LEGACY_SET_MODE: return "LEGACY_SET_MODE";
        case UARTCommand::LEGACY_PING: return "LEGACY_PING";
        case UARTCommand::NONE:
        default:
            return "NONE";
    }
}

// Handshake management methods
void UARTController::sendBootAck() {
    sendResponse(RSP_BOOT_ACK);
    bootHandshakeComplete = true;
    LOG_INFO(TAG, "Boot handshake acknowledged");
}

void UARTController::sendFabricAck() {
    sendResponse(RSP_FABRIC_ACK);
    fabricHandshakeComplete = true;
    LOG_INFO(TAG, "Fabric handshake acknowledged");
}

bool UARTController::isBootHandshakeComplete() const {
    return bootHandshakeComplete;
}

bool UARTController::isFabricHandshakeComplete() const {
    return fabricHandshakeComplete;
}

// Text command parsing
bool UARTController::parseTextCommand(const char* text, size_t length) {
    if (length == 0) return false;
    
    // Handle basic proximity commands
    if (strcmp(text, "NEAR") == 0) {
        lastCommand = UARTCommand::NEAR_MOTION_TRIGGER;
        LOG_INFO(TAG, "Text command: NEAR");
        return true;
    }
    
    if (strcmp(text, "FAR") == 0) {
        lastCommand = UARTCommand::FAR_MOTION_TRIGGER;
        LOG_INFO(TAG, "Text command: FAR");
        return true;
    }
    
    // Handle state commands in format [STATE_NAME]
    if (length >= 3 && text[0] == '[' && text[length - 1] == ']') {
        // Extract state name (remove brackets)
        char stateName[64];
        size_t stateLen = length - 2;
        if (stateLen >= sizeof(stateName)) stateLen = sizeof(stateName) - 1;
        memcpy(stateName, text + 1, stateLen);
        stateName[stateLen] = '\0';
        
        // Map common state names to commands
        if (strcmp(stateName, "PLAY_WELCOME") == 0) {
            lastCommand = UARTCommand::PLAY_WELCOME;
        } else if (strcmp(stateName, "WAIT_FOR_NEAR") == 0) {
            lastCommand = UARTCommand::WAIT_FOR_NEAR;
        } else if (strcmp(stateName, "PLAY_FINGER_PROMPT") == 0) {
            lastCommand = UARTCommand::PLAY_FINGER_PROMPT;
        } else if (strcmp(stateName, "MOUTH_OPEN_WAIT_FINGER") == 0) {
            lastCommand = UARTCommand::MOUTH_OPEN_WAIT_FINGER;
        } else if (strcmp(stateName, "FINGER_DETECTED") == 0) {
            lastCommand = UARTCommand::FINGER_DETECTED;
        } else if (strcmp(stateName, "SNAP_WITH_FINGER") == 0) {
            lastCommand = UARTCommand::SNAP_WITH_FINGER;
        } else if (strcmp(stateName, "SNAP_NO_FINGER") == 0) {
            lastCommand = UARTCommand::SNAP_NO_FINGER;
        } else if (strcmp(stateName, "FORTUNE_FLOW") == 0) {
            lastCommand = UARTCommand::FORTUNE_FLOW;
        } else if (strcmp(stateName, "FORTUNE_DONE") == 0) {
            lastCommand = UARTCommand::FORTUNE_DONE;
        } else if (strcmp(stateName, "COOLDOWN") == 0) {
            lastCommand = UARTCommand::COOLDOWN;
        } else {
            LOG_WARN(TAG, "Unknown state command: [%s]", stateName);
            return false;
        }
        
        LOG_INFO(TAG, "Text command: [%s] -> %s", stateName, commandToString(lastCommand));
        return true;
    }
    
    return false;
}

// Send response frame
void UARTController::sendResponse(uint8_t responseCmd) {
    uint8_t frame[4];
    frame[0] = FRAME_START;
    frame[1] = 1; // Length (command only)
    frame[2] = responseCmd;
    frame[3] = calculateCRC8(&frame[1], 2); // CRC for length + command
    
    Serial1.write(frame, 4);
    LOG_INFO(TAG, "Sent response: 0x%02X", responseCmd);
}
