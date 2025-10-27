#ifndef SERVO_TESTER_H
#define SERVO_TESTER_H

/*
 * ServoTester - Find servo limits by testing with microsecond precision
 * 
 * ============================================================================
 * HOW TO ADD SERVO TESTER TO YOUR PROJECT
 * ============================================================================
 * 
 * 1. Include the files in your project:
 *    - Copy servo_tester.h and servo_tester.cpp to your src/ directory
 *    - Ensure you have ESP32Servo library installed (via platformio.ini)
 * 
 * 2. Add to your main.cpp:
 * 
 *    // At top with other includes:
 *    #include "servo_tester.h"
 * 
 *    // In global declarations:
 *    ServoTester servoTest;
 * 
 *    // In setup() - BEFORE normal servo initialization:
 *    Serial.begin(115200);
 *    delay(500);
 *    LoggingManager::instance().begin(&Serial);  // If you use LoggingManager
 *    servoTest.initialize(SERVO_PIN, 1500);       // pin, neutral µs (typically 1500)
 *    // Banner prints automatically, servo wiggles to verify communication
 * 
 *    // In loop() - Add servo update:
 *    servoTest.update();  // Updates sweep animations (non-blocking)
 * 
 *    // In your serial command handler:
 *    if (cmd.startsWith("servo")) {
 *        servoTest.processCommand(cmd);  // Handles "servo <offset>" or "servo"
 *    }
 * 
 * 3. To disable servo testing and restore normal operation:
 *    - Comment out or remove the servoTest.initialize() call
 *    - Comment out servoTest.update() in loop()
 *    - Remove servo command handling
 * 
 * ============================================================================
 * HOW TO FIND SERVO SAFE LIMITS (HUMAN INSTRUCTIONS)
 * ============================================================================
 * 
 * STEP 1: Setup
 *    - Upload code with ServoTester enabled
 *    - Connect serial monitor (115200 baud, raw mode recommended)
 *    - Watch for startup banner showing commands
 *    - Verify servo wiggles on startup (±100 µs test)
 * 
 * STEP 2: Find MINIMUM limit (servo moving in negative direction)
 *    - Start conservative: Type "servo -100" and observe smooth movement
 *    - If smooth, try larger negative offsets: -200, -300, -400, etc.
 *    - Continue increasing until servo:
 *        * Stops moving completely
 *        * Struggles/stutters
 *        * Makes grinding/clicking sounds
 *        * Reaches mechanical end stop
 *    - Your MIN limit is the last offset that worked smoothly
 *    - Example: If "-600" works but "-700" struggles, MIN = neutral - 600
 * 
 * STEP 3: Find MAXIMUM limit (servo moving in positive direction)
 *    - Return to neutral: Type "servo" (no offset)
 *    - Start conservative: Type "servo 100" and observe smooth movement
 *    - If smooth, try larger positive offsets: 200, 300, 400, etc.
 *    - Continue until servo shows signs of limit (same as MIN testing)
 *    - Your MAX limit is the last offset that worked smoothly
 *    - Example: If "700" works but "800" struggles, MAX = neutral + 700
 * 
 * STEP 4: Calculate safe range for your servo code
 *    - Safe MIN = neutral - (last working negative offset)
 *    - Safe MAX = neutral + (last working positive offset)
 *    - Example with 1500 µs neutral:
 *        * If MIN limit found at -600: safe_min = 1500 - 600 = 900 µs
 *        * If MAX limit found at +700: safe_max = 1500 + 700 = 2200 µs
 * 
 * STEP 5: Apply limits to your servo controller
 *    - Use these values in your servo controller's min/max settings
 *    - Add 50-100 µs safety margin for production code
 *    - Store in config file or as constants
 * 
 * COMMANDS:
 *    "servo -100"  → Sweep from neutral to (neutral-100) and back
 *    "servo 200"   → Sweep from neutral to (neutral+200) and back  
 *    "servo"       → Immediately return to neutral position
 * 
 * TIPS:
 *    - Test both directions separately (don't mix min/max in one command)
 *    - Return to neutral between tests for consistency
 *    - Listen for unusual sounds indicating mechanical stress
 *    - Watch for smooth motion vs. jerky/stuttering movement
 *    - If servo makes noise but moves, you're close to limit
 *    - If servo completely stops, you've exceeded the limit
 */

#include <Arduino.h>
#include <Servo.h>

class ServoTester {
private:
    Servo servo;
    int servoPin;
    int neutralUs;        // Neutral position in microseconds (default: 1500)
    int currentUs;        // Current position in microseconds
    bool isSweeping;      // Whether a sweep is in progress
    unsigned long sweepStartTime;
    int sweepDurationMs;  // Duration of one-way sweep in milliseconds
    int targetUs;         // Target microsecond value for current sweep
    int sweepStartUs;     // Starting position for current sweep
    
public:
    ServoTester();
    void initialize(int pin, int neutralMicroseconds = 1500);
    void setNeutral(int neutralMicroseconds);
    int getNeutral() const { return neutralUs; }
    
    // Sweep from neutral to target offset and back
    // offsetUs: offset from neutral (negative = less than neutral, positive = more than neutral)
    // durationMs: duration for one-way sweep (default: 1000ms)
    void sweepFromNeutral(int offsetUs, int durationMs = 1000);
    
    // Set position directly in microseconds (no sweep)
    void setPositionUs(int microseconds);
    
    // Update the sweep animation (call this in loop())
    void update();
    
    // Check if currently sweeping
    bool isCurrentlySweeping() const { return isSweeping; }
    
    // Get current position in microseconds
    int getCurrentUs() const { return currentUs; }
    
    // Stop any ongoing sweep and return to neutral
    void returnToNeutral();
    
    // Process a serial command (call this from your command handler)
    // Returns true if command was handled, false otherwise
    // Supports: "servo <offset>" or "servo"
    bool processCommand(const String& cmd);
    
    // Print startup banner (called automatically by initialize())
    void printBanner();
};

#endif // SERVO_TESTER_H
