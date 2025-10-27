#ifndef UART_CONTROLLER_H
#define UART_CONTROLLER_H

#include <Arduino.h>

enum class UARTCommand {
    NONE,
    FAR_MOTION_TRIGGER,
    NEAR_MOTION_TRIGGER,
    PLAY_WELCOME,
    WAIT_FOR_NEAR,
    PLAY_FINGER_PROMPT,
    MOUTH_OPEN_WAIT_FINGER,
    FINGER_DETECTED,
    SNAP_WITH_FINGER,
    SNAP_NO_FINGER,
    FORTUNE_FLOW,
    FORTUNE_DONE,
    COOLDOWN,
    LEGACY_SET_MODE,
    LEGACY_PING
};

class UARTController {
public:
    UARTController();
    void begin();
    void update();
    UARTCommand getLastCommand();
    void clearLastCommand();
    static const char* commandToString(UARTCommand command);

private:
    static constexpr uint8_t FRAME_START = 0xA5;
    static constexpr uint8_t CMD_FAR_MOTION_TRIGGER = 0x01;
    static constexpr uint8_t CMD_NEAR_MOTION_TRIGGER = 0x02;
    static constexpr uint8_t CMD_PLAY_WELCOME = 0x03;
    static constexpr uint8_t CMD_WAIT_FOR_NEAR = 0x04;
    static constexpr uint8_t CMD_PLAY_FINGER_PROMPT = 0x05;
    static constexpr uint8_t CMD_MOUTH_OPEN_WAIT_FINGER = 0x06;
    static constexpr uint8_t CMD_FINGER_DETECTED = 0x07;
    static constexpr uint8_t CMD_SNAP_WITH_FINGER = 0x08;
    static constexpr uint8_t CMD_SNAP_NO_FINGER = 0x09;
    static constexpr uint8_t CMD_FORTUNE_FLOW = 0x0A;
    static constexpr uint8_t CMD_FORTUNE_DONE = 0x0B;
    static constexpr uint8_t CMD_COOLDOWN = 0x0C;
    static constexpr uint8_t CMD_LEGACY_SET_MODE = 0x20;
    static constexpr uint8_t CMD_LEGACY_PING = 0x21;
    
    static constexpr int UART_BAUD = 115200;
    static constexpr int RX_PIN = 22;
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
