/**
 * ============================================================================
 * ARCHIVED EXAMPLE — not used by current build
 * ---------------------------------------------------------------------------
 * This header represents an older/simplified hardware mapping example. The
 * active, supported mapping for the final build lives in docs/hardware.md and
 * docs/perfboard-assembly.md, and the firmware pin constants in src/main.cpp.
 * ============================================================================
 *
 * @file death_circuit_v5.h
 * @brief Death Fortune Teller - Hardware Configuration v5.0
 * @author Death Circuit Project
 * @date 2025
 * 
 * Revision 5.0 Features:
 * - All 3 LEDs clustered on adjacent pins (12-14) for single connector
 * - Servo separated with standard 3-pin connector
 * - Minimal 16-wire design maintained
 * - Single 4-wire cable to skull for all LEDs
 */

#ifndef DEATH_CIRCUIT_V5_H
#define DEATH_CIRCUIT_V5_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// ============================================================================
// OPTIMIZED PIN ASSIGNMENTS v5.0
// All LEDs grouped together for single connector!
// ============================================================================

// ALL LEDs - Single 3-Pin Cluster (Physical pins 12-14)
// Use a single 4-wire cable to skull (3 signals + ground)
namespace LEDs {
    constexpr uint8_t LEFT_EYE  = 27;  // GPIO27 - Pin 12 - PWM Channel 0
    constexpr uint8_t RIGHT_EYE = 14;  // GPIO14 - Pin 13 - PWM Channel 1
    constexpr uint8_t MOUTH     = 12;  // GPIO12 - Pin 14 - PWM Channel 2
    
    // PWM Configuration
    constexpr uint8_t CH_LEFT   = 0;   // PWM channel for left eye
    constexpr uint8_t CH_RIGHT  = 1;   // PWM channel for right eye
    constexpr uint8_t CH_MOUTH  = 2;   // PWM channel for mouth
    constexpr uint32_t FREQ     = 5000; // 5kHz PWM frequency
    constexpr uint8_t RESOLUTION = 8;   // 8-bit (0-255)
}

// Servo Motor - Separate Standard Connector (Physical pin 15)
namespace ServoMotor {
    constexpr uint8_t PIN       = 13;   // GPIO13 - Pin 15 - Separate from LEDs
    constexpr uint8_t CH        = 3;    // PWM channel for servo
    constexpr uint32_t FREQ     = 50;   // 50Hz for standard servo
    constexpr uint8_t RESOLUTION = 16;  // 16-bit for precision
    constexpr uint16_t MIN_US  = 1000;  // 1ms pulse = closed
    constexpr uint16_t MAX_US  = 2000;  // 2ms pulse = open
    constexpr uint16_t MID_US  = 1500;  // 1.5ms pulse = middle
}

// Thermal Printer UART1 (Physical pins 10-11) 
namespace Printer {
    constexpr uint8_t RX_PIN   = 25;    // GPIO25 - Pin 10 - From printer TX
    constexpr uint8_t TX_PIN   = 26;    // GPIO26 - Pin 11 - To printer RX
    constexpr uint32_t BAUD    = 9600;  // Standard thermal printer baud
    constexpr uint8_t UART_NUM = 1;     // Hardware UART1
}

// SD Card SPI Interface (Physical pins 22-24,27)
namespace SDCard {
    constexpr uint8_t CS   = 5;   // GPIO5  - Pin 22 - Chip Select
    constexpr uint8_t SCK  = 18;  // GPIO18 - Pin 23 - Clock (VSPI)
    constexpr uint8_t MISO = 19;  // GPIO19 - Pin 24 - Data In (VSPI)
    constexpr uint8_t MOSI = 23;  // GPIO23 - Pin 27 - Data Out (VSPI)
}

// Matter Communication UART2 (Physical pins 20-21)
namespace Matter {
    constexpr uint8_t RX_PIN   = 16;     // GPIO16 - Pin 20 - From C3 TX
    constexpr uint8_t TX_PIN   = 17;     // GPIO17 - Pin 21 - To C3 RX
    constexpr uint32_t BAUD    = 115200; // High-speed Matter protocol
    constexpr uint8_t UART_NUM = 2;      // Hardware UART2
}

// Touch Sensor (Physical pin 19)
namespace Touch {
    constexpr uint8_t SENSOR       = 4;  // GPIO4 - Pin 19 - Touch0 channel
    constexpr uint16_t THRESHOLD   = 40; // Adjust for your copper foil
    constexpr uint16_t DEBOUNCE_MS = 30; // Prevent false triggers
}

// Future I2C Expansion (Physical pins 25-26)
namespace I2C {
    constexpr uint8_t SDA = 21;  // GPIO21 - Pin 25
    constexpr uint8_t SCL = 22;  // GPIO22 - Pin 26
}

// ============================================================================
// DEATH CIRCUIT CONTROLLER CLASS
// ============================================================================

class DeathCircuit {
private:
    static bool initialized;
    static uint32_t lastTouchTime;
    static uint16_t lastTouchValue;
    static uint8_t ledBrightness[3];  // Store current brightness for each LED
    
public:
    /**
     * @brief Initialize all hardware subsystems
     * @return true if successful, false on error
     */
    static bool begin() {
        Serial.begin(115200);
        delay(100);
        
        Serial.println("\n╔══════════════════════════════════════╗");
        Serial.println("║  DEATH CIRCUIT FORTUNE TELLER v5.0  ║");
        Serial.println("║    All LEDs in Single Connector!    ║");
        Serial.println("╚══════════════════════════════════════╝\n");
        
        bool success = true;
        
        // Initialize subsystems
        success &= initPins();
        success &= initPWM();
        success &= initUARTs();
        success &= initSPI();
        success &= initTouch();
        
        if (success) {
            Serial.println("✓ All systems initialized successfully!");
            playStartupSequence();
        } else {
            Serial.println("✗ Initialization failed - check connections!");
        }
        
        initialized = success;
        return success;
    }
    
    // ===== UNIFIED LED CONTROL =====
    
    /**
     * @brief Set individual LED brightness
     * @param led LED index (0=left eye, 1=right eye, 2=mouth)
     * @param brightness 0-255
     */
    static void setLED(uint8_t led, uint8_t brightness) {
        if (led > 2) return;
        
        uint8_t pins[3] = {LEDs::LEFT_EYE, LEDs::RIGHT_EYE, LEDs::MOUTH};
        uint8_t channels[3] = {LEDs::CH_LEFT, LEDs::CH_RIGHT, LEDs::CH_MOUTH};
        
        ledBrightness[led] = brightness;
        ledcWrite(channels[led], brightness);
    }
    
    // Convenient LED control methods
    static void setLeftEye(uint8_t brightness)  { setLED(0, brightness); }
    static void setRightEye(uint8_t brightness) { setLED(1, brightness); }
    static void setMouth(uint8_t brightness)    { setLED(2, brightness); }
    
    /**
     * @brief Set all LEDs to same brightness
     * @param brightness 0-255
     */
    static void setAllLEDs(uint8_t brightness) {
        setLeftEye(brightness);
        setRightEye(brightness);
        setMouth(brightness);
    }
    
    /**
     * @brief Set all LEDs with individual values
     * Useful for animations
     */
    static void setLEDs(uint8_t left, uint8_t right, uint8_t mouth) {
        setLeftEye(left);
        setRightEye(right);
        setMouth(mouth);
    }
    
    // LED Effects
    
    static void eyesGlow(uint8_t maxBrightness = 255, uint16_t duration = 2000) {
        uint16_t stepDelay = duration / (maxBrightness / 5) / 2;
        
        // Fade in
        for (int i = 0; i <= maxBrightness; i += 5) {
            setLeftEye(i);
            setRightEye(i);
            delay(stepDelay);
        }
        
        // Fade out
        for (int i = maxBrightness; i >= 0; i -= 5) {
            setLeftEye(i);
            setRightEye(i);
            delay(stepDelay);
        }
    }
    
    static void spookyFlicker() {
        for (int i = 0; i < 10; i++) {
            uint8_t brightness = random(50, 255);
            setAllLEDs(brightness);
            delay(random(30, 100));
        }
        setAllLEDs(0);
    }
    
    static void deathStare() {
        // Eyes bright, mouth dim
        setLEDs(255, 255, 50);
        delay(2000);
        
        // Quick blink
        setLEDs(0, 0, 50);
        delay(100);
        setLEDs(255, 255, 50);
        delay(1000);
        
        // Fade all
        for (int i = 255; i >= 0; i -= 5) {
            setLEDs(i, i, i/5);
            delay(20);
        }
    }
    
    // ===== SERVO CONTROL =====
    
    static void setJawAngle(uint8_t angle) {
        angle = constrain(angle, 0, 180);
        uint32_t pulse = map(angle, 0, 180, ServoMotor::MIN_US, ServoMotor::MAX_US);
        uint32_t duty = (pulse * 65535) / 20000; // Convert to 16-bit duty @ 50Hz
        ledcWrite(ServoMotor::CH, duty);
    }
    
    static void jawOpen()   { setJawAngle(180); }
    static void jawClosed() { setJawAngle(0); }
    static void jawMid()    { setJawAngle(90); }
    
    static void jawChatter(uint8_t times = 3, uint16_t speed = 100) {
        for (uint8_t i = 0; i < times; i++) {
            jawOpen();
            setMouth(255);  // Mouth lights up when open
            delay(speed);
            
            jawClosed();
            setMouth(0);    // Mouth dark when closed
            delay(speed);
        }
        jawMid();
        setMouth(128);  // Mouth dim at rest
    }
    
    static void speakingAnimation(uint16_t duration = 3000) {
        uint32_t startTime = millis();
        
        while (millis() - startTime < duration) {
            // Random jaw movements
            setJawAngle(random(20, 160));
            
            // Mouth LED flickers with speech
            setMouth(random(100, 255));
            
            delay(random(50, 150));
        }
        
        jawMid();
        setMouth(0);
    }
    
    // ===== TOUCH SENSOR =====
    
    static bool isTouched() {
        uint16_t touchValue = touchRead(Touch::SENSOR);
        
        // Debouncing
        if (millis() - lastTouchTime < Touch::DEBOUNCE_MS) {
            return false;
        }
        
        // Touch detected if value drops below threshold
        if (touchValue < Touch::THRESHOLD) {
            lastTouchTime = millis();
            lastTouchValue = touchValue;
            return true;
        }
        
        return false;
    }
    
    static uint16_t getTouchValue() {
        return touchRead(Touch::SENSOR);
    }
    
    // ===== MATTER COMMUNICATION =====
    
    static void sendToMatter(const String& message) {
        Serial2.println(message);
    }
    
    static bool matterAvailable() {
        return Serial2.available();
    }
    
    static String readFromMatter() {
        return Serial2.readStringUntil('\n');
    }
    
    // ===== THERMAL PRINTER =====
    
    static void printFortune(const String& fortune) {
        // Initialize printer
        Serial1.print("\x1B\x40");
        delay(50);
        
        // Bold mode on
        Serial1.print("\x1B\x45\x01");
        
        // Center align
        Serial1.print("\x1B\x61\x01");
        
        // Print header with skull
        Serial1.println("╔════════════════════╗");
        Serial1.println("║   ☠ DEATH FORTUNE ☠   ║");
        Serial1.println("╚════════════════════╝");
        Serial1.println();
        
        // Regular text for fortune
        Serial1.print("\x1B\x45\x00");  // Bold off
        Serial1.println(fortune);
        Serial1.println();
        
        // Footer
        Serial1.println("━━━━━━━━━━━━━━━━━━━━");
        Serial1.println("Your fate is sealed!");
        Serial1.println();
        
        // Feed paper
        Serial1.print("\n\n\n");
    }
    
    // ===== SD CARD =====
    
    static bool mountSD() {
        if (!SD.begin(SDCard::CS)) {
            Serial.println("✗ SD Card mount failed!");
            return false;
        }
        Serial.println("✓ SD Card mounted");
        return true;
    }
    
    // ===== DIAGNOSTICS =====
    
    static void runDiagnostics() {
        Serial.println("\n=== RUNNING DIAGNOSTICS ===\n");
        
        testAllLEDs();
        testServo();
        testTouch();
        testUARTs();
        testSDCard();
        
        Serial.println("\n=== DIAGNOSTICS COMPLETE ===\n");
    }
    
private:
    // Initialization functions
    
    static bool initPins() {
        // Configure LED outputs (all adjacent!)
        pinMode(LEDs::LEFT_EYE, OUTPUT);
        pinMode(LEDs::RIGHT_EYE, OUTPUT);
        pinMode(LEDs::MOUTH, OUTPUT);
        
        // Configure servo
        pinMode(ServoMotor::PIN, OUTPUT);
        
        // Configure SD CS
        pinMode(SDCard::CS, OUTPUT);
        digitalWrite(SDCard::CS, HIGH); // Deselect SD
        
        Serial.println("✓ Pins configured");
        return true;
    }
    
    static bool initPWM() {
        // Initialize all LED PWM channels (clustered)
        ledcSetup(LEDs::CH_LEFT, LEDs::FREQ, LEDs::RESOLUTION);
        ledcSetup(LEDs::CH_RIGHT, LEDs::FREQ, LEDs::RESOLUTION);
        ledcSetup(LEDs::CH_MOUTH, LEDs::FREQ, LEDs::RESOLUTION);
        
        ledcAttachPin(LEDs::LEFT_EYE, LEDs::CH_LEFT);
        ledcAttachPin(LEDs::RIGHT_EYE, LEDs::CH_RIGHT);
        ledcAttachPin(LEDs::MOUTH, LEDs::CH_MOUTH);
        
        // Initialize servo PWM (separate)
        ledcSetup(ServoMotor::CH, ServoMotor::FREQ, ServoMotor::RESOLUTION);
        ledcAttachPin(ServoMotor::PIN, ServoMotor::CH);
        
        Serial.println("✓ PWM channels configured");
        Serial.println("  - LEDs: GPIO 27,14,12 (pins 12-14) clustered");
        Serial.println("  - Servo: GPIO 13 (pin 15) separate");
        return true;
    }
    
    static bool initUARTs() {
        // Matter UART2
        Serial2.begin(Matter::BAUD, SERIAL_8N1, Matter::RX_PIN, Matter::TX_PIN);
        
        // Printer UART1
        Serial1.begin(Printer::BAUD, SERIAL_8N1, Printer::RX_PIN, Printer::TX_PIN);
        
        Serial.println("✓ UARTs initialized");
        return true;
    }
    
    static bool initSPI() {
        SPI.begin(SDCard::SCK, SDCard::MISO, SDCard::MOSI, SDCard::CS);
        Serial.println("✓ SPI initialized");
        return true;
    }
    
    static bool initTouch() {
        lastTouchValue = touchRead(Touch::SENSOR);
        Serial.printf("✓ Touch sensor baseline: %d\n", lastTouchValue);
        return true;
    }
    
    // Test functions
    
    static void testAllLEDs() {
        Serial.println("Testing LED cluster (single connector)...");
        
        // Test pattern showing all LEDs are on one connector
        Serial.println("  Testing left eye (pin 12)...");
        setLeftEye(255);
        delay(300);
        setLeftEye(0);
        
        Serial.println("  Testing right eye (pin 13)...");
        setRightEye(255);
        delay(300);
        setRightEye(0);
        
        Serial.println("  Testing mouth (pin 14)...");
        setMouth(255);
        delay(300);
        setMouth(0);
        
        Serial.println("  All LEDs together...");
        setAllLEDs(255);
        delay(500);
        
        // Fun pattern
        for (int i = 0; i < 3; i++) {
            setLEDs(255, 0, 0);    // Left only
            delay(100);
            setLEDs(0, 255, 0);    // Right only
            delay(100);
            setLEDs(0, 0, 255);    // Mouth only
            delay(100);
        }
        
        setAllLEDs(0);
        Serial.println("  ✓ LED cluster test complete");
    }
    
    static void testServo() {
        Serial.println("Testing servo (separate connector)...");
        
        jawClosed();
        delay(500);
        jawMid();
        delay(500);
        jawOpen();
        delay(500);
        jawMid();
        
        Serial.println("  ✓ Servo test complete");
    }
    
    static void testTouch() {
        Serial.println("Testing touch sensor...");
        Serial.println("  Touch the copper foil now...");
        
        uint32_t startTime = millis();
        while (millis() - startTime < 3000) {
            uint16_t val = getTouchValue();
            if (val < Touch::THRESHOLD) {
                Serial.printf("  ✓ Touch detected! Value: %d\n", val);
                // Visual feedback on touch
                setAllLEDs(255);
                delay(100);
                setAllLEDs(0);
                return;
            }
            delay(50);
        }
        
        Serial.printf("  ⚠ No touch detected. Current value: %d\n", getTouchValue());
    }
    
    static void testUARTs() {
        Serial.println("Testing UARTs...");
        
        Serial2.println("PING_C3");
        delay(100);
        if (Serial2.available()) {
            Serial.println("  ✓ Matter UART responsive");
        } else {
            Serial.println("  ⚠ Matter UART no response");
        }
        
        Serial1.print("\x1B\x40");
        delay(100);
        Serial.println("  ✓ Printer UART command sent");
    }
    
    static void testSDCard() {
        Serial.println("Testing SD card...");
        
        if (mountSD()) {
            File root = SD.open("/");
            if (root) {
                Serial.println("  ✓ SD card readable");
                root.close();
            }
        }
    }
    
    static void playStartupSequence() {
        Serial.println("\n♫ Playing startup sequence...");
        
        // All LEDs fade in together (showing they're clustered)
        for (int i = 0; i <= 255; i += 5) {
            setAllLEDs(i);
            delay(10);
        }
        
        // Spooky effect
        jawChatter(2, 100);
        deathStare();
        
        Serial.println("  Ready for fortunes!\n");
    }
};

// Static member initialization
bool DeathCircuit::initialized = false;
uint32_t DeathCircuit::lastTouchTime = 0;
uint16_t DeathCircuit::lastTouchValue = 0;
uint8_t DeathCircuit::ledBrightness[3] = {0, 0, 0};

// ============================================================================
// CONVENIENCE MACROS
// ============================================================================

// LED control (all on single connector!)
#define ALL_LEDS_ON()   DeathCircuit::setAllLEDs(255)
#define ALL_LEDS_OFF()  DeathCircuit::setAllLEDs(0)
#define EYES_ON()       DeathCircuit::setLEDs(255, 255, 0)
#define EYES_OFF()      DeathCircuit::setLEDs(0, 0, ledBrightness[2])
#define MOUTH_FLASH()   DeathCircuit::setMouth(255); delay(100); DeathCircuit::setMouth(0)

// Servo control (separate connector)
#define JAW_OPEN()      DeathCircuit::jawOpen()
#define JAW_CLOSE()     DeathCircuit::jawClosed()
#define JAW_CHATTER()   DeathCircuit::jawChatter()

// Touch detection
#define IS_TOUCHED()    DeathCircuit::isTouched()

#endif // DEATH_CIRCUIT_V5_H

/**
 * Example Usage:
 * 
 * #include "death_circuit_v5.h"
 * 
 * void setup() {
 *     if (!DeathCircuit::begin()) {
 *         while(1); // Halt on init failure
 *     }
 *     
 *     DeathCircuit::runDiagnostics();
 * }
 * 
 * void loop() {
 *     if (IS_TOUCHED()) {
 *         // User touched the copper foil!
 *         ALL_LEDS_ON();
 *         JAW_CHATTER();
 *         DeathCircuit::speakingAnimation(2000);
 *         DeathCircuit::printFortune("The spirits say: Use a single LED connector!");
 *         DeathCircuit::deathStare();
 *         ALL_LEDS_OFF();
 *     }
 *     
 *     // Idle animation
 *     static uint32_t lastGlow = 0;
 *     if (millis() - lastGlow > 10000) {
 *         DeathCircuit::eyesGlow(100, 3000);
 *         lastGlow = millis();
 *     }
 *     
 *     delay(50);
 * }
 */
