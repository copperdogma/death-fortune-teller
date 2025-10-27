#include "servo_tester.h"
#include "logging_manager.h"

static constexpr const char* TAG = "ServoTester";

ServoTester::ServoTester()
    : servoPin(-1), neutralUs(1500), currentUs(1500), 
      isSweeping(false), sweepDurationMs(1000), targetUs(1500), sweepStartUs(1500) {}

void ServoTester::initialize(int pin, int neutralMicroseconds) {
    servoPin = pin;
    neutralUs = neutralMicroseconds;
    currentUs = neutralMicroseconds;
    
    // Print banner showing commands and neutral position
    printBanner();
    
    // Attach with explicit min/max microseconds to allow full range at 50 Hz
    servo.attach(servoPin,
                Servo::CHANNEL_NOT_ATTACHED,
                Servo::DEFAULT_MIN_ANGLE,
                Servo::DEFAULT_MAX_ANGLE,
                500,
                2500,
                50);
    
    LOG_INFO(TAG, "ServoTester initialized on pin %d, neutral: %d µs", servoPin, neutralUs);
    
    // Test sequence: wiggle ±100 µs to verify servo communication
    isSweeping = false;
    
    int testOffset = 100;
    servo.writeMicroseconds(neutralUs);
    delay(1000);
    
    servo.writeMicroseconds(neutralUs - testOffset);
    delay(1000);
    
    servo.writeMicroseconds(neutralUs + testOffset);
    delay(1000);
    
    servo.writeMicroseconds(neutralUs);
    delay(500);
    
    Serial.println("✅ Servo test mode ready!");
}

void ServoTester::printBanner() {
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║  🔧 SERVO TEST MODE ACTIVE            ║");
    Serial.println("║  All normal operation disabled        ║");
    Serial.println("║                                       ║");
    Serial.println("║  SERVO COMMANDS:                      ║");
    Serial.print("║    'servo -100' → "); Serial.print(neutralUs); Serial.print("→"); Serial.print(neutralUs - 100); Serial.print("→"); Serial.print(neutralUs); Serial.println(" µs   ║");
    Serial.print("║    'servo 200'  → "); Serial.print(neutralUs); Serial.print("→"); Serial.print(neutralUs + 200); Serial.print("→"); Serial.print(neutralUs); Serial.println(" µs   ║");
    Serial.print("║    'servo'      → return to "); Serial.print(neutralUs); Serial.println(" µs   ║");
    Serial.println("║                                       ║");
    Serial.println("║  Use 'help' for all commands          ║");
    Serial.println("╚═══════════════════════════════════════╝\n");
}

void ServoTester::setNeutral(int neutralMicroseconds) {
    neutralUs = neutralMicroseconds;
    LOG_INFO(TAG, "Neutral position set to %d µs", neutralUs);
}

void ServoTester::sweepFromNeutral(int offsetUs, int durationMs) {
    if (isSweeping) {
        LOG_WARN(TAG, "Sweep already in progress, ignoring new sweep command");
        return;
    }
    
    // Calculate target from neutral + offset
    targetUs = neutralUs + offsetUs;
    sweepStartUs = neutralUs;
    sweepDurationMs = durationMs;
    sweepStartTime = millis();
    isSweeping = true;
    
    LOG_INFO(TAG, "Starting sweep: %d µs → %d µs (offset: %d µs, duration: %d ms)", 
             neutralUs, targetUs, offsetUs, durationMs);
}

void ServoTester::setPositionUs(int microseconds) {
    currentUs = microseconds;
    if (servoPin >= 0) {
        servo.writeMicroseconds(microseconds);
    }
}

void ServoTester::update() {
    if (!isSweeping) {
        return;
    }
    
    unsigned long elapsed = millis() - sweepStartTime;
    
    // Phase 1: Move from neutral to target (0 to durationMs)
    if (elapsed < sweepDurationMs) {
        float progress = (float)elapsed / sweepDurationMs;
        int us = sweepStartUs + (int)((targetUs - sweepStartUs) * progress);
        setPositionUs(us);
    }
    // Phase 2: Move from target back to neutral (durationMs to 2*durationMs)
    else if (elapsed < 2 * sweepDurationMs) {
        float progress = (float)(elapsed - sweepDurationMs) / sweepDurationMs;
        int us = targetUs - (int)((targetUs - sweepStartUs) * progress);
        setPositionUs(us);
    }
    // Phase 3: Complete - return to neutral and stop sweeping
    else {
        setPositionUs(neutralUs);
        isSweeping = false;
        LOG_INFO(TAG, "Sweep complete, returned to neutral (%d µs)", neutralUs);
    }
}

void ServoTester::returnToNeutral() {
    isSweeping = false;
    setPositionUs(neutralUs);
    LOG_INFO(TAG, "Returned to neutral (%d µs)", neutralUs);
}

bool ServoTester::processCommand(const String& cmd) {
    String cmdLower = cmd;
    cmdLower.toLowerCase();
    cmdLower.trim();
    
    if (!cmdLower.startsWith("servo")) {
        return false; // Not a servo command
    }
    
    // Parse "servo <offset>" command
    int spaceIndex = cmdLower.indexOf(' ');
    if (spaceIndex > 0) {
        String offsetStr = cmdLower.substring(spaceIndex + 1);
        offsetStr.trim();
        int offsetUs = offsetStr.toInt();
        
        Serial.print(">>> Servo sweep: offset = "); Serial.print(offsetUs); Serial.println(" µs");
        sweepFromNeutral(offsetUs, 1000);  // 1 second sweep
    } else {
        // "servo" without value returns to neutral
        Serial.print(">>> Returning servo to neutral ("); Serial.print(neutralUs); Serial.println(" µs)");
        returnToNeutral();
    }
    
    return true; // Command was handled
}
