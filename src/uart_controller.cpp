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
    if (Serial1.available()) {
        uint8_t buffer[RX_BUFFER_SIZE];
        size_t length = Serial1.readBytes(buffer, min((size_t)Serial1.available(), (size_t)RX_BUFFER_SIZE));
        
        if (parseFrame(buffer, length)) {
            lastCommandTime = millis();
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
    if (length < 4) return false; // Minimum frame size
    
    // Look for start byte
    for (size_t i = 0; i < length - 3; i++) {
        if (buffer[i] == FRAME_START) {
            uint8_t len = buffer[i + 1];
            if (i + len + 3 < length) { // Check if we have enough data
                uint8_t cmd = buffer[i + 2];
                uint8_t* payload = &buffer[i + 3];
                uint8_t crc = buffer[i + 3 + len - 1];
                
                // Verify CRC
                uint8_t calculatedCRC = calculateCRC8(&buffer[i + 1], len + 1);
                if (calculatedCRC == crc) {
                    lastCommand = commandFromByte(cmd);
                    return true;
                }
            }
        }
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
        case CMD_SET_MODE: return UARTCommand::SET_MODE;
        case CMD_TRIGGER_FAR: return UARTCommand::TRIGGER_FAR;
        case CMD_TRIGGER_NEAR: return UARTCommand::TRIGGER_NEAR;
        case CMD_PING: return UARTCommand::PING;
        default: return UARTCommand::NONE;
    }
}
