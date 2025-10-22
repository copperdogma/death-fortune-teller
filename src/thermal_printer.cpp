#include "thermal_printer.h"

ThermalPrinter::ThermalPrinter(int txPin, int rxPin) 
    : txPin(txPin), rxPin(rxPin), initialized(false), hasErrorState(false), lastCommandTime(0) {
}

void ThermalPrinter::begin() {
    Serial1.begin(PRINTER_BAUD, SERIAL_8N1, rxPin, txPin);
    initialized = true;
    Serial.println("Thermal printer initialized");
}

void ThermalPrinter::update() {
    if (!initialized) return;
    
    // Check for timeout on commands
    if (lastCommandTime > 0 && millis() - lastCommandTime > COMMAND_TIMEOUT_MS) {
        handleError();
    }
}

bool ThermalPrinter::printFortune(const String& fortune) {
    if (!initialized || hasErrorState) return false;
    
    Serial.printf("Printing fortune: %s\n", fortune.c_str());
    
    // Print logo first
    if (!printLogo()) {
        return false;
    }
    
    // Print fortune text
    Serial1.println(fortune);
    Serial1.println();
    Serial1.println("--- Death's Fortune ---");
    Serial1.println();
    
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
    Serial1.println("    DEATH'S FORTUNE    ");
    Serial1.println("   ╔═══════════════╗   ");
    Serial1.println("   ║               ║   ");
    Serial1.println("   ║   ☠️  DEATH  ☠️   ║   ");
    Serial1.println("   ║               ║   ");
    Serial1.println("   ╚═══════════════╝   ");
    Serial1.println();
    
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
    Serial1.write(cmd);
}

void ThermalPrinter::sendCommand(uint8_t cmd, uint8_t param) {
    if (!initialized) return;
    Serial1.write(cmd);
    Serial1.write(param);
}

void ThermalPrinter::sendCommand(uint8_t cmd, uint8_t* data, size_t length) {
    if (!initialized) return;
    Serial1.write(cmd);
    Serial1.write(data, length);
}

bool ThermalPrinter::waitForResponse(unsigned long timeoutMs) {
    unsigned long startTime = millis();
    while (millis() - startTime < timeoutMs) {
        if (Serial1.available()) {
            return true;
        }
        delay(10);
    }
    return false;
}

void ThermalPrinter::handleError() {
    hasErrorState = true;
    Serial.println("Thermal printer error - check paper and connection");
}
