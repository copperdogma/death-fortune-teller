#ifndef THERMAL_PRINTER_H
#define THERMAL_PRINTER_H

#include <Arduino.h>

class ThermalPrinter {
public:
    ThermalPrinter(int txPin, int rxPin);
    void begin();
    void update();
    bool printFortune(const String& fortune);
    bool printLogo();
    bool isReady();
    bool hasError();

private:
    int txPin;
    int rxPin;
    static constexpr int PRINTER_BAUD = 9600;
    
    bool initialized;
    bool hasErrorState;
    unsigned long lastCommandTime;
    static constexpr unsigned long COMMAND_TIMEOUT_MS = 5000;
    
    void sendCommand(uint8_t cmd);
    void sendCommand(uint8_t cmd, uint8_t param);
    void sendCommand(uint8_t cmd, uint8_t* data, size_t length);
    bool waitForResponse(unsigned long timeoutMs = 1000);
    void handleError();
};

#endif // THERMAL_PRINTER_H
