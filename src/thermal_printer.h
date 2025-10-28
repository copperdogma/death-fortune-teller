#ifndef THERMAL_PRINTER_H
#define THERMAL_PRINTER_H

#include <Arduino.h>
#include <HardwareSerial.h>

class ThermalPrinter {
public:
    ThermalPrinter(HardwareSerial &serialPort, int txPin, int rxPin, int baud = 9600);
    void begin();
    void update();
    bool printFortune(const String& fortune);
    bool printLogo();
    bool isReady();
    bool hasError();

private:
    HardwareSerial &serial;
    int txPin;
    int rxPin;
    int printerBaud;
    
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
