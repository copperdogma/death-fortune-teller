#ifndef THERMAL_PRINTER_H
#define THERMAL_PRINTER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <FS.h>

class ThermalPrinter {
public:
    ThermalPrinter(HardwareSerial &serialPort, int txPin, int rxPin, int baud = 9600);
    void begin();
    void update();
    bool printFortune(const String& fortune);
    bool printLogo();
    bool isReady();
    bool hasError();
    void setLogoPath(const String &path);
    bool printTestPage();

private:
    HardwareSerial &serial;
    int txPin;
    int rxPin;
    int printerBaud;
    String logoPath;
    
    bool initialized;
    bool hasErrorState;
    unsigned long lastCommandTime;
    static constexpr unsigned long COMMAND_TIMEOUT_MS = 5000;
    static constexpr uint16_t PRINTER_MAX_WIDTH_DOTS = 384;
    static constexpr uint8_t DEFAULT_LINE_SPACING_DOTS = 32;
    static constexpr size_t MAX_TEXT_COLUMNS = 32;
    
    void sendCommand(uint8_t cmd);
    void sendCommand(uint8_t cmd, uint8_t param);
    void sendCommand(uint8_t cmd, uint8_t* data, size_t length);
    bool waitForResponse(unsigned long timeoutMs = 1000);
    void handleError();
    void sendInitSequence();
    void setJustification(uint8_t mode);
    void setLineSpacing(uint8_t dots);
    void setDefaultLineSpacing();
    void feedLines(uint8_t count);
    bool printBitmapLogo();
    bool printBitmapFromFile(File &file);
    bool streamBitmap(File &file, int32_t width, int32_t height, uint32_t dataOffset, bool invert);
    bool printTextLogoFallback();
    void printWrappedLine(const String &line);
    void printFortuneBody(const String &fortune);
};

#endif // THERMAL_PRINTER_H
