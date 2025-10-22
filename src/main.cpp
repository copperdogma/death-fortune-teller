/*
  Death, the Fortune-Telling Skeleton
  Main file for controlling an animatronic skull that reads fortunes.
  Based on TwoSkulls but adapted for single skull with Matter control.
*/

#include <Arduino.h>
#include "audio_player.h"
#include "bluetooth_controller.h"
#include "servo_controller.h"
#include "sd_card_manager.h"
#include "skull_audio_animator.h"
#include "config_manager.h"
#include "uart_controller.h"
#include "thermal_printer.h"
#include "finger_sensor.h"
#include "fortune_generator.h"
#include "BluetoothA2DPSource.h"
#include "esp_a2dp_api.h"
#include "esp_attr.h"

// Pin definitions
const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED
const int SERVO_PIN = 15;      // Servo control pin
const int MOUTH_LED_PIN = 2;   // Mouth LED pin
const int CAP_SENSE_PIN = 4;   // Capacitive finger sensor pin
const int PRINTER_TX_PIN = 21; // Thermal printer TX pin
const int PRINTER_RX_PIN = 20; // Thermal printer RX pin

// Global objects
LightController lightController(LEFT_EYE_PIN, RIGHT_EYE_PIN);
ServoController servoController;
SDCardManager *sdCardManager = nullptr;
AudioPlayer *audioPlayer = nullptr;
BluetoothController *bluetoothController = nullptr;
UARTController *uartController = nullptr;
ThermalPrinter *thermalPrinter = nullptr;
FingerSensor *fingerSensor = nullptr;
FortuneGenerator *fortuneGenerator = nullptr;
SDCardContent sdCardContent;
bool isPrimary = true;
String initializationAudioPath;
bool initializationQueued = false;
bool initializationPlayed = false;

int32_t IRAM_ATTR provideAudioFramesThunk(void *context, Frame *frame, int32_t frameCount) {
    AudioPlayer *player = static_cast<AudioPlayer *>(context);
    if (!player) {
        return 0;
    }
    return player->provideAudioFrames(frame, frameCount);
}

// Function declarations
void handleUARTCommand(UARTCommand cmd);
void startWelcomeSequence();
void startFortuneFlow();
void handleFortuneFlow(unsigned long currentTime);

// State machine
enum class DeathState {
    IDLE,
    PLAY_WELCOME,
    FORTUNE_FLOW,
    COOLDOWN
};

DeathState currentState = DeathState::IDLE;
unsigned long stateStartTime = 0;
unsigned long lastTriggerTime = 0;
const unsigned long TRIGGER_DEBOUNCE_MS = 2000;

// Fortune flow state
bool mouthOpen = false;
bool fingerDetected = false;
unsigned long fingerDetectionStart = 0;
unsigned long snapDelayStart = 0;
int snapDelayMs = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("\n\nDeath Starting...");
    
    // Initialize light controller
    lightController.begin();
    lightController.blinkEyes(3); // Startup blink
    
    // Initialize servo
    servoController.initialize(SERVO_PIN, 0, 80);
    
    // Initialize SD card
    sdCardManager = new SDCardManager();
    while (!sdCardManager->begin()) {
        Serial.println("SD Card mount failed! Retrying...");
        lightController.blinkEyes(3);
        delay(500);
    }
    Serial.println("✅ SD Card mounted successfully!");
    sdCardContent = sdCardManager->loadContent();
    
    // Load configuration
    ConfigManager &config = ConfigManager::getInstance();
    while (!config.loadConfig()) {
        Serial.println("Failed to load config. Retrying...");
        lightController.blinkEyes(5);
        delay(500);
    }
    Serial.println("✅ Configuration loaded successfully!");

    String role = config.getRole();
    if (role.length() == 0 || role.equalsIgnoreCase("primary")) {
        isPrimary = true;
        Serial.println("Role configured as PRIMARY");
    } else if (role.equalsIgnoreCase("secondary")) {
        isPrimary = false;
        Serial.println("Role configured as SECONDARY");
    } else {
        Serial.printf("Unknown role '%s'. Defaulting to PRIMARY.\n", role.c_str());
        isPrimary = true;
    }

    initializationAudioPath = isPrimary ? sdCardContent.primaryInitAudio : sdCardContent.secondaryInitAudio;
    if (initializationAudioPath.length() == 0 && sdCardContent.primaryInitAudio.length() > 0) {
        initializationAudioPath = sdCardContent.primaryInitAudio;
    }
    if (initializationAudioPath.length() == 0) {
        initializationAudioPath = "/audio/Initialized - Primary.wav";
        Serial.println("Initialization audio fallback: /audio/Initialized - Primary.wav");
    } else {
        Serial.printf("Initialization audio discovered: %s\n", initializationAudioPath.c_str());
    }
    
    // Initialize other components
    audioPlayer = new AudioPlayer(*sdCardManager);
    audioPlayer->setPlaybackStartCallback([](const String &filePath) {
        Serial.printf("▶️ Audio playback started: %s\n", filePath.c_str());
        if (filePath.equals(initializationAudioPath)) {
            initializationQueued = false;
        }
    });
    audioPlayer->setPlaybackEndCallback([](const String &filePath) {
        Serial.printf("⏹ Audio playback finished: %s\n", filePath.c_str());
        if (filePath.equals(initializationAudioPath)) {
            initializationPlayed = true;
        }
    });
    bluetoothController = new BluetoothController();
    uartController = new UARTController();
    thermalPrinter = new ThermalPrinter(PRINTER_TX_PIN, PRINTER_RX_PIN);
    fingerSensor = new FingerSensor(CAP_SENSE_PIN);
    fortuneGenerator = new FortuneGenerator();
    Serial.println("✅ All components initialized successfully!");
    
    // Initialize Bluetooth A2DP
    String speakerName = config.getBluetoothSpeakerName();
    bluetoothController->initializeA2DP(speakerName, provideAudioFramesThunk, audioPlayer);
    bluetoothController->setConnectionStateChangeCallback([](int state) {
        if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            bool isPlaying = audioPlayer && audioPlayer->isAudioPlaying();
            bool hasQueue = audioPlayer && audioPlayer->hasQueuedAudio();
            Serial.printf("🔗 Bluetooth speaker connected. initPlayed=%s, isAudioPlaying=%s, hasQueued=%s\n",
                          initializationPlayed ? "true" : "false",
                          isPlaying ? "true" : "false",
                          hasQueue ? "true" : "false");
            if (!initializationPlayed && audioPlayer && !initializationQueued) {
                Serial.println("🎬 Priming initialization audio after Bluetooth connect");
                audioPlayer->playNext(initializationAudioPath);
                initializationQueued = true;
            }
        } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            Serial.println("🔌 Bluetooth speaker disconnected.");
        }
    });

    bluetoothController->set_volume(config.getSpeakerVolume());
    
    // Start connection retry logic
    bluetoothController->startConnectionRetry();
    
    // Play initialization audio
    if (initializationAudioPath.length() > 0 && sdCardManager->fileExists(initializationAudioPath.c_str())) {
        audioPlayer->playNext(initializationAudioPath);
        Serial.printf("🎵 Queued initialization audio: %s\n", initializationAudioPath.c_str());
        initializationQueued = true;
        initializationPlayed = false;
    } else {
        Serial.printf("⚠️ Initialization audio missing: %s\n", initializationAudioPath.c_str());
    }
    
    Serial.println("🎉 Death initialized successfully!");
}

void loop() {
    unsigned long currentTime = millis();
    static unsigned long lastStatusLogTime = 0;
    
    // Update all controllers
    if (bluetoothController) bluetoothController->update();
    if (audioPlayer) audioPlayer->update();
    if (uartController) uartController->update();
    if (thermalPrinter) thermalPrinter->update();
    if (fingerSensor) fingerSensor->update();
    
    // Periodic status logging (every 5 seconds)
    if (currentTime - lastStatusLogTime >= 5000) {
        size_t freeHeap = ESP.getFreeHeap();
        Serial.printf("Status: Free mem: %d bytes, ", freeHeap);
        Serial.printf("A2DP connected: %s, ", bluetoothController->isA2dpConnected() ? "true" : "false");
        Serial.printf("Retrying: %s, ", bluetoothController->isRetryingConnection() ? "true" : "false");
        Serial.printf("Audio playing: %s, ", audioPlayer->isAudioPlaying() ? "true" : "false");
        Serial.printf("Init queued: %s, Init played: %s\n",
                      initializationQueued ? "true" : "false",
                      initializationPlayed ? "true" : "false");
        lastStatusLogTime = currentTime;
    }
    
    // Handle UART commands
    if (uartController) {
        UARTCommand cmd = uartController->getLastCommand();
        if (cmd != UARTCommand::NONE) {
            handleUARTCommand(cmd);
        }
    }
    
    // State machine
    switch (currentState) {
        case DeathState::IDLE:
            // Wait for triggers
            break;
            
        case DeathState::PLAY_WELCOME:
            if (!audioPlayer->isAudioPlaying()) {
                currentState = DeathState::IDLE;
            }
            break;
            
        case DeathState::FORTUNE_FLOW:
            handleFortuneFlow(currentTime);
            break;
            
        case DeathState::COOLDOWN:
            if (currentTime - stateStartTime >= 12000) { // 12 second cooldown
                currentState = DeathState::IDLE;
            }
            break;
    }
    
    delay(1);
}

void handleUARTCommand(UARTCommand cmd) {
    unsigned long currentTime = millis();
    
    // Debounce check
    if (currentTime - lastTriggerTime < TRIGGER_DEBOUNCE_MS) {
        return;
    }
    
    // Ignore commands while busy
    if (currentState != DeathState::IDLE) {
        Serial.println("Ignoring command - system busy");
        return;
    }
    
    switch (cmd) {
        case UARTCommand::TRIGGER_FAR:
            startWelcomeSequence();
            break;
            
        case UARTCommand::TRIGGER_NEAR:
            startFortuneFlow();
            break;
            
        default:
            break;
    }
    
    lastTriggerTime = currentTime;
}

void startWelcomeSequence() {
    Serial.println("Starting welcome sequence");
    currentState = DeathState::PLAY_WELCOME;
    stateStartTime = millis();
    
    // Play random welcome skit
    String welcomeFile = "/audio/welcome/welcome_01.wav"; // TODO: Random selection
    audioPlayer->playNext(welcomeFile);
}

void startFortuneFlow() {
    Serial.println("Starting fortune flow");
    currentState = DeathState::FORTUNE_FLOW;
    stateStartTime = millis();
    
    // Play fortune prompt
    String promptFile = "/audio/fortune/fortune_01.wav"; // TODO: Random selection
    audioPlayer->playNext(promptFile);
    
    // Open mouth and start LED pulse
    servoController.setPosition(80); // Open mouth
    mouthOpen = true;
    lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
}

void handleFortuneFlow(unsigned long currentTime) {
    // Check for finger detection
    if (fingerSensor && fingerSensor->isFingerDetected()) {
        if (!fingerDetected) {
            fingerDetected = true;
            fingerDetectionStart = currentTime;
            Serial.println("Finger detected!");
        }
        
        // Check if finger has been detected for required duration (120ms)
        if (currentTime - fingerDetectionStart >= 120) {
            if (snapDelayMs == 0) {
                // Start random snap delay (1-3 seconds)
                snapDelayMs = random(1000, 3001);
                snapDelayStart = currentTime;
                Serial.printf("Starting snap delay: %dms\n", snapDelayMs);
            }
        }
    } else {
        fingerDetected = false;
    }
    
    // Check snap delay timeout
    if (snapDelayMs > 0 && currentTime - snapDelayStart >= snapDelayMs) {
        // Snap the jaw (release servo)
        servoController.interruptMovement();
        mouthOpen = false;
        
        // Generate and print fortune
        String fortune = fortuneGenerator->generateFortune();
        thermalPrinter->printFortune(fortune);
        
        // Continue with fortune monologue
        String monologueFile = "/audio/fortune/fortune_02.wav"; // TODO: Random selection
        audioPlayer->playNext(monologueFile);
        
        // Move to cooldown state
        currentState = DeathState::COOLDOWN;
        stateStartTime = currentTime;
    }
    
    // Check for timeout (6 seconds max wait)
    if (currentTime - stateStartTime >= 6000) {
        // Timeout - close mouth and end
        servoController.setPosition(0);
        mouthOpen = false;
        currentState = DeathState::COOLDOWN;
        stateStartTime = currentTime;
    }
}
