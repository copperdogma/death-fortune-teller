#include "uart_controller.h"
#include "logging_manager.h"

static constexpr const char* TAG = "UART";

UARTController::UARTController() 
    : lastCommand(UARTCommand::NONE), lastCommandTime(0) {
}

void UARTController::begin() {
    Serial1.begin(UART_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial1.setRxBufferSize(RX_BUFFER_SIZE);
    LOG_INFO(TAG, "UART controller initialized");
}

void UARTController::update() {
    int availableBytes = Serial1.available();
    if (availableBytes > 0) {
        uint8_t buffer[RX_BUFFER_SIZE];
        size_t toRead = min(static_cast<size_t>(availableBytes), static_cast<size_t>(RX_BUFFER_SIZE));
        size_t length = Serial1.readBytes(buffer, toRead);
        if (parseFrame(buffer, length)) {
            lastCommandTime = millis();
        } else {
            if (length > 0) {
                char sample[64] = {0};
                size_t sampleLen = min(length, static_cast<size_t>(8));
                for (size_t i = 0; i < sampleLen; ++i) {
                    snprintf(sample + (i * 3), sizeof(sample) - (i * 3), "%02X ", buffer[i]);
                }
                LOG_WARN(TAG, "UART bytes read=%u but no valid frame parsed (sample: %s)", static_cast<unsigned>(length), sample);
            }
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
        case UARTCommand::LEGACY_SET_MODE: return "LEGACY_SET_MODE";
        case UARTCommand::LEGACY_PING: return "LEGACY_PING";
        case UARTCommand::NONE:
        default:
            return "NONE";
    }
}
