#include "thermal_printer.h"
#include "logging_manager.h"

static constexpr const char* TAG = "ThermalPrinter";

ThermalPrinter::ThermalPrinter(HardwareSerial &serialPort, int txPin, int rxPin, int baud) 
    : serial(serialPort), txPin(txPin), rxPin(rxPin),
      printerBaud((baud < 1200 || baud > 115200) ? 9600 : baud),
      initialized(false), hasErrorState(false), lastCommandTime(0) {
}

void ThermalPrinter::begin() {
    serial.begin(printerBaud, SERIAL_8N1, rxPin, txPin);
    initialized = true;
    LOG_INFO(TAG, "Thermal printer initialized");
}

void ThermalPrinter::update() {
    if (!initialized) return;
    
    // Check for timeout on commands
    if (lastCommandTime > 0 && millis() - lastCommandTime > COMMAND_TIMEOUT_MS) {
        if (!hasErrorState) {
            handleError();
        }
        lastCommandTime = 0;
    }
}

bool ThermalPrinter::printFortune(const String& fortune) {
    if (!initialized || hasErrorState) return false;
    
    LOG_INFO(TAG, "Printing fortune: %s", fortune.c_str());
    
    // Print logo first
    if (!printLogo()) {
        return false;
    }
    
    // Print fortune text
    serial.println(fortune);
    serial.println();
    serial.println("--- Death's Fortune ---");
    serial.println();
    
    // Feed paper
    sendCommand(0x0A); // Line feed
    sendCommand(0x0A); // Line feed
    sendCommand(0x0A); // Line feed
    
    lastCommandTime = millis();
    return true;
}

bool ThermalPrinter::printLogo() {
    if (!initialized || hasErrorState) return false;
    
    // TODO: Implement logo printing from BMP file
    // For now, just print a text logo
    serial.println("    DEATH'S FORTUNE    ");
    serial.println("   ╔═══════════════╗   ");
    serial.println("   ║               ║   ");
    serial.println("   ║   ☠️  DEATH  ☠️   ║   ");
    serial.println("   ║               ║   ");
    serial.println("   ╚═══════════════╝   ");
    serial.println();
    
    return true;
}

bool ThermalPrinter::isReady() {
    return initialized && !hasErrorState;
}

bool ThermalPrinter::hasError() {
    return hasErrorState;
}

void ThermalPrinter::sendCommand(uint8_t cmd) {
    if (!initialized) return;
    serial.write(cmd);
}

void ThermalPrinter::sendCommand(uint8_t cmd, uint8_t param) {
    if (!initialized) return;
    serial.write(cmd);
    serial.write(param);
}

void ThermalPrinter::sendCommand(uint8_t cmd, uint8_t* data, size_t length) {
    if (!initialized) return;
    serial.write(cmd);
    serial.write(data, length);
}

bool ThermalPrinter::waitForResponse(unsigned long timeoutMs) {
    unsigned long startTime = millis();
    while (millis() - startTime < timeoutMs) {
        if (serial.available()) {
            return true;
        }
        delay(10);
    }
    return false;
}

void ThermalPrinter::handleError() {
    if (hasErrorState) {
        return;
    }
    hasErrorState = true;
    LOG_ERROR(TAG, "Thermal printer error - check paper and connection");
}
