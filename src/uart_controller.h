#ifndef UART_CONTROLLER_H
#define UART_CONTROLLER_H

#include <Arduino.h>

enum class UARTCommand {
    NONE,
    SET_MODE,
    TRIGGER_FAR,
    TRIGGER_NEAR,
    PING
};

class UARTController {
public:
    UARTController();
    void begin();
    void update();
    UARTCommand getLastCommand();
    void clearLastCommand();

private:
    static constexpr uint8_t FRAME_START = 0xA5;
    static constexpr uint8_t CMD_SET_MODE = 0x02;
    static constexpr uint8_t CMD_TRIGGER_FAR = 0x05;
    static constexpr uint8_t CMD_TRIGGER_NEAR = 0x06;
    static constexpr uint8_t CMD_PING = 0x04;
    
    static constexpr int UART_BAUD = 115200;
    static constexpr int RX_PIN = 20;
    static constexpr int TX_PIN = 21;
    static constexpr int RX_BUFFER_SIZE = 1024;
    
    UARTCommand lastCommand;
    unsigned long lastCommandTime;
    static constexpr unsigned long COMMAND_TIMEOUT_MS = 100;
    
    bool parseFrame(uint8_t* buffer, size_t length);
    uint8_t calculateCRC8(uint8_t* data, size_t length);
    UARTCommand commandFromByte(uint8_t cmd);
};

#endif // UART_CONTROLLER_H

