/*
  Two Chatting Skulls with Skull Jaws Synced to Audio

  Main file for controlling two animatronic skulls that chat with each other.
  This system uses Bluetooth for communication, SD card for audio storage,
  and various sensors and actuators for animation.
*/
#include <Arduino.h>
#include "bluetooth_controller.h"
#include "FS.h"     // Ensure ESP32 board package is installed
#include "SD.h"     // Ensure ESP32 board package is installed
#include "light_controller.h"
#include "servo_controller.h"
#include "sd_card_manager.h"
#include "skull_audio_animator.h"
#include "audio_player.h"
#include "config_manager.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include <esp_coexist.h>
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "skit_selector.h"

const int LEFT_EYE_PIN = 32;  // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33; // GPIO pin for right eye LED
LightController lightController(LEFT_EYE_PIN, RIGHT_EYE_PIN);

const int VOLUME_DIVISOR = 1; // FOR DEBUGGING: divide volume by this amount to set volume lower

// GPIO trigger constants and variables
const int MATTER_TRIGGER_PIN = 1;  // GPIO 1 for Matter controller trigger
volatile bool matterTriggerDetected = false;  // Flag for interrupt handler

SDCardManager *sdCardManager = nullptr;
SDCardContent sdCardContent;

bool isPrimary = false; // Determines if this skull is the primary or secondary unit
bool isDonePlayingInitializationAudio = false;
bool isBleInitializationStarted = false;
ServoController servoController;
bluetooth_controller bluetoothController;
AudioPlayer *audioPlayer = nullptr;
static unsigned long lastCharacteristicUpdateMillis = 0;
static unsigned long lastServerScanMillis = 0;

SkullAudioAnimator *skullAudioAnimator = nullptr;

// Audio processing parameters, enabling exponential smoothing
struct AudioState
{
  double smoothedPosition = 0;
  double maxObservedRMS = 0;
  bool isJawClosed = true;
  int chunkCounter = 0;
};

AudioState audioState;

// Constants for audio processing and jaw movement
const double ALPHA = 0.2;             // Smoothing factor; 0-1, lower=smoother
const double SILENCE_THRESHOLD = 200; // Higher = vol needs to be louder to be considered "not silence", max 2000?
const int MIN_MOVEMENT_THRESHOLD = 3; // Minimum degree change to move the servo
const double MOVE_EXPONENT = 0.2;     // Exponent for non-linear mapping of audio to jaw movement. 0-1, smaller = more movement
const double RMS_MAX = 32768.0;       // Maximum possible RMS value for 16-bit audio
const int CHUNKS_NEEDED = 17;         // Chunks requested per call. Note: it's 34 chunks per 100ms of audio

const int SERVO_PIN = 15; // Servo control pin

esp_adc_cal_characteristics_t adc_chars;

SkitSelector *skitSelector = nullptr;

// Declare these variables outside the loop
static unsigned long lastTimeAudioPlayed = 0;
static const unsigned long AUDIO_COOLDOWN_TIME = 10000; // 10 seconds cooldown after audio ends

// Add these variables near the top of the file, with other global variables
unsigned long lastJawMovementTime = 0;
const unsigned long BREATHING_INTERVAL = 7000; // 7 seconds in milliseconds
const int BREATHING_JAW_ANGLE = 30;            // 30 degrees opening
const int BREATHING_MOVEMENT_DURATION = 2000;  // 2 seconds for the movement

// Custom crash handler to provide debug information and restart the device
void custom_crash_handler()
{
  Serial.println("detected!");
  Serial.printf("Free memory at crash: %d bytes\n", ESP.getFreeHeap());

  // Print some basic debug info
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  Serial.printf("ESP32 Chip Revision: %d\n", chip_info.revision);
  Serial.printf("CPU Cores: %d\n", chip_info.cores);
  Serial.printf("Flash Size: %d MB\n", spi_flash_get_chip_size() / (1024 * 1024));

  // Print last error
  Serial.printf("Last error: %s\n", esp_err_to_name(esp_get_minimum_free_heap_size()));

  // Flush serial output
  Serial.flush();

  // Wait a bit before restarting
  delay(1000);

  // Restart the ESP32
  esp_restart();
}

// GPIO interrupt handler for Matter controller trigger
void IRAM_ATTR matterTriggerInterrupt() {
  matterTriggerDetected = true;
}

// Secondary (server) only: Handle connection state changes
void onConnectionStateChange(ConnectionState newState)
{
  Serial.printf("Connection state changed to: %s\n", bluetoothController.getConnectionStateString(newState).c_str());

  if (!isPrimary && newState == ConnectionState::CONNECTED)
  {
    if (bluetoothController.isA2dpConnected() && !audioPlayer->isAudioPlaying())
    {
      audioPlayer->playNext("/audio/Polo.wav");
    }
  }
}

// Secondary (server) only: Handle characteristic changes
void onCharacteristicChange(const std::string &newValue)
{
  Serial.printf("Characteristic changed. New value: %s\n", newValue.c_str());

  // Attempt to play the audio file specified by the new characteristic value
  if (bluetoothController.isA2dpConnected() && !audioPlayer->isAudioPlaying())
  {
    audioPlayer->playNext(newValue.c_str());
    Serial.printf("Attempting to play audio file: %s\n", newValue.c_str());
  }
  else
  {
    Serial.println("Cannot play audio: Either Bluetooth is not connected or audio is already playing.");
  }
}

// This is called whenever SkullAudioAnimator determines this skull is speaking or not speaking.
// If it's not speaking we want to mute the audio. That way, although both skulls are playing the same audio
// in sync, they'll only be speaking their individual parts.
void onSpeakingStateChange(bool isSpeaking)
{
  if (audioPlayer)
  {
    audioPlayer->setMuted(!isSpeaking);
  }
}

bool onCharacteristicChangeRequest(const std::string &value)
{
  // Check if we can play the audio file
  if (audioPlayer->isAudioPlaying())
  {
    Serial.println("Cannot play new audio: Already playing");
    return false;
  }

  if (!sdCardManager->fileExists(value.c_str()))
  {
    Serial.printf("Cannot play new audio: File not found: %s\n", value.c_str());
    return false;
  }

  // Add any other necessary checks here
  // For example, you might want to check if the file is a valid audio format,
  // or if there's enough free memory to play the file, etc.

  return true;
}

// Update the breathing jaw movement function
void breathingJawMovement()
{
  if (!audioPlayer->isAudioPlaying())
  {
    servoController.smoothMove(BREATHING_JAW_ANGLE, BREATHING_MOVEMENT_DURATION);
    delay(100); // Short pause at the open position
    servoController.smoothMove(0, BREATHING_MOVEMENT_DURATION);
  }
}

// Main setup function
void setup()
{
  Serial.begin(115200);

  // Register custom crash handler
  esp_register_shutdown_handler((shutdown_handler_t)custom_crash_handler);

  Serial.println("\n\n\n\n\n\nStarting setup 20250723 ... ");

  // Initialize light controller first for blinking
  lightController.begin();

  // Initialize servo with default values (can be updated later if needed)
  servoController.initialize(SERVO_PIN, 0, 80); // Default min=0, max=80 degrees

  // Initialize SD Card Manager
  sdCardManager = new SDCardManager();

  // Attempt to initialize the SD card until successful
  while (!sdCardManager->begin())
  {
    Serial.println("MAIN: SD Card: Mount Failed! Retrying...");
    lightController.blinkEyes(3); // 3 blinks for SD card failure
    delay(500);
  }

  // Load SD card content
  sdCardContent = sdCardManager->loadContent();
  if (sdCardContent.skits.empty())
  {
    Serial.println("MAIN: No skits found on SD card.");
  }

  // Now that SD card is initialized, load configuration
  ConfigManager &config = ConfigManager::getInstance();
  bool configLoaded = false;

  while (!configLoaded)
  {
    configLoaded = config.loadConfig();
    if (!configLoaded)
    {
      Serial.println("MAIN: Failed to load configuration. Retrying...");
      lightController.blinkEyes(5); // 5 blinks for config file failure
      delay(500);
    }
  }

  // Configuration loaded successfully, now we can use it
  String bluetoothSpeakerName = config.getBluetoothSpeakerName();
  String role = config.getRole();
  int speakerVolume = config.getSpeakerVolume() / VOLUME_DIVISOR;
  int servoMinDegrees = config.getServoMinDegrees();
  int servoMaxDegrees = config.getServoMaxDegrees();

  // Initialize SkitSelector with parsed skits
  skitSelector = new SkitSelector(sdCardContent.skits);

  // Update servo with config values (if different from defaults)
  servoController.setMinMaxDegrees(servoMinDegrees, servoMaxDegrees);

  // Determine role based on settings.txt
  if (role.equals("primary"))
  {
    isPrimary = true;
    lightController.blinkEyes(4); // Blink eyes 4 times for Primary
    Serial.println("MAIN: This skull is configured as PRIMARY");
  }
  else if (role.equals("secondary"))
  {
    isPrimary = false;
    lightController.blinkEyes(2); // Blink eyes twice for Secondary
    Serial.println("MAIN: This skull is configured as SECONDARY");
  }
  else
  {
    lightController.blinkEyes(2); // Blink eyes twice for Secondary
    Serial.printf("MAIN: Invalid role in settings.txt ('%s'). Defaulting to SECONDARY\n", role.c_str());
    isPrimary = false;
  }

  // Initialize AudioPlayer
  esp_coex_preference_set(ESP_COEX_PREFER_WIFI);

  audioPlayer = new AudioPlayer(*sdCardManager);

  // Initialize Bluetooth A2DP only
  bluetoothController.initializeA2DP(bluetoothSpeakerName, [](Frame *frame, int32_t frame_count)
                                     { return audioPlayer->provideAudioFrames(frame, frame_count); });

  // Queue the initialization audio
  String initAudioFilePath = isPrimary ? "/audio/Initialized - Primary.wav" : "/audio/Initialized - Secondary.wav";
  audioPlayer->playNext(initAudioFilePath);

  // Announce "System initialized" and role
  Serial.printf("MAIN: Playing initialization audio: %s\n", initAudioFilePath.c_str());
  Serial.printf("MAIN: Queued initialization audio: %s\n", initAudioFilePath.c_str());

  // TESTING CODE:
  // Queue the "Skit - names" skit to play next
  // ParsedSkit namesSkit = sdCardManager->findSkitByName(sdCardContent.skits, "Skit - names");
  // audioPlayer->playNext(namesSkit.audioFile.c_str());
  // Serial.printf("'Skit - names' found; queueing audio: %s\n", namesSkit.audioFile.c_str());

  // Initialize GPIO trigger pin
  pinMode(MATTER_TRIGGER_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(MATTER_TRIGGER_PIN), matterTriggerInterrupt, RISING);
  Serial.println("MAIN: GPIO trigger initialized on pin " + String(MATTER_TRIGGER_PIN));

  // Initialize Bluetooth after AudioPlayer.
  // Include the callback so that the bluetooth_controller library can call the AudioPlayer's
  // provideAudioFrames method to get more audio data when the bluetooth speaker needs it.
  bluetoothController.set_volume(speakerVolume);

  // Set the initial state of the eyes to dim
  lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);

  // ADC initialization (used in loop() to calculate voltage)
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  audioPlayer->setPlaybackStartCallback([](const String &filePath)
                                        { Serial.printf("MAIN: Started playing audio: %s\n", filePath.c_str()); });

  audioPlayer->setPlaybackEndCallback([](const String &filePath)
                                      { 
                                        lastTimeAudioPlayed = millis(); // Set the last time audio played
                                        Serial.printf("MAIN: Finished playing audio: %s at time %lu\n", filePath.c_str(), lastTimeAudioPlayed);
                                        if (filePath.endsWith("/audio/Initialized - Primary.wav") || filePath.endsWith("/audio/Initialized - Secondary.wav"))
                                        {
                                          isDonePlayingInitializationAudio = true;
                                        } 
                                        if (skullAudioAnimator != nullptr)
                                        {
                                          skullAudioAnimator->setPlaybackEnded(filePath);
                                        }
                                        if (skitSelector != nullptr) {
                                            skitSelector->updateSkitPlayCount(filePath);
                                        } });

  audioPlayer->setAudioFramesProvidedCallback([](const String &filePath, const Frame *frames, int32_t frameCount)
                                              {
                                                if (skullAudioAnimator != nullptr)
                                                {
                                                    unsigned long playbackTime = audioPlayer->getPlaybackTime();
                                                    skullAudioAnimator->processAudioFrames(frames, frameCount, filePath, playbackTime);
                                                } });

  // Set the connection state change callback
  bluetoothController.setConnectionStateChangeCallback(onConnectionStateChange);

  // Set the characteristic change callback
  bluetoothController.setCharacteristicChangeCallback(onCharacteristicChange);

  // Initialize SkullAudioAnimator
  skullAudioAnimator = new SkullAudioAnimator(isPrimary, servoController, lightController, sdCardContent.skits, *sdCardManager,
                                              servoMinDegrees, servoMaxDegrees);
  skullAudioAnimator->setSpeakingStateCallback(onSpeakingStateChange);

  // Set the characteristic change request callback
  bluetoothController.setCharacteristicChangeRequestCallback(onCharacteristicChangeRequest);
}

// Main loop function
void loop()
{
  unsigned long currentMillis = millis();
  static unsigned long lastStateLoggingMillis = 0;

  // Reset the watchdog timer to prevent system resets
  esp_task_wdt_reset();

  // Update audio player state
  bool isAudioPlaying = audioPlayer->isAudioPlaying();

  if (isAudioPlaying)
  {
    lastTimeAudioPlayed = millis(); // Set the last time audio played
  }

  // Periodic logging of system state (every 5 seconds)
  if (currentMillis - lastStateLoggingMillis >= 5000)
  {
    size_t freeHeap = ESP.getFreeHeap();
    esp_reset_reason_t reset_reason = esp_reset_reason();

    // Read ADC and convert to voltage
    uint32_t adc_reading = adc1_get_raw(ADC1_CHANNEL_0);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);

    Serial.printf("%lu loop(): Free mem: %d bytes, ", currentMillis, freeHeap);
    Serial.printf("BT connected: %s, ", bluetoothController.isA2dpConnected() ? "true" : "false");
    Serial.printf("isAudioPlaying: %s, ", isAudioPlaying ? "true" : "false");
    Serial.printf("BLE clientIsConnectedToServer: %s, ", bluetoothController.clientIsConnectedToServer() ? "true" : "false");
    Serial.printf("BLE serverHasClientConnected: %s, ", bluetoothController.serverHasClientConnected() ? "true" : "false");
    Serial.printf("Voltage: %d mV, ", voltage);
    Serial.printf("\n");

    if (reset_reason == ESP_RST_BROWNOUT)
    {
      Serial.println("WARNING: Last reset was due to brownout!");
    }

    lastStateLoggingMillis = currentMillis;
  }

  // // A2DP + BLE TEST TEST CODE: play the Names skit after everything is properly initialized
  // if (isPrimary && currentMillis - lastCharacteristicUpdateMillis >= 10000 && bluetoothController.clientIsConnectedToServer() &&
  //     bluetoothController.isA2dpConnected() && !audioPlayer->isAudioPlaying())
  // {
  //   String message = "/audio/Skit - milkshakes.wav";
  //   bool success = bluetoothController.setRemoteCharacteristicValue(message.c_str());
  //   if (success)
  //   {
  //     Serial.printf("MAIN: Successfully updated BLE characteristic with message: %s\n", message.c_str());

  //     // Play the same file immediatly outselves. It should be fast enough to be in sync with Secondary.
  //     audioPlayer->playNext(message);
  //   }
  //   else
  //   {
  //     Serial.printf("MAIN: Failed to update BLE characteristic with message: %s\n", message.c_str());
  //   }
  //   lastCharacteristicUpdateMillis = currentMillis + 30000; // extra delay so it doens't play them back to back
  // }

  // // SKULL_AUDIO_ANIMATOR TEST CODE: play the Names skit after A2DP is properly initialized
  // if (bluetoothController.isA2dpConnected() && !audioPlayer->isAudioPlaying() && currentMillis - lastCharacteristicUpdateMillis >= 10000)
  // {
  //   audioPlayer->playNext("/audio/Skit - names.wav");
  //   lastCharacteristicUpdateMillis = currentMillis;
  // }

  // Priamry Only: Play "Marco!" ever 5 seconds when scanning for the BLE server (Secondary skull)
  static unsigned long lastScanLogMillis = 0;
  if (isPrimary && bluetoothController.getConnectionState() == ConnectionState::SCANNING)
  {
    if (currentMillis - lastScanLogMillis >= 5000)
    {
      if (bluetoothController.isA2dpConnected() && !audioPlayer->isAudioPlaying())
      {
        audioPlayer->playNext("/audio/Marco.wav");
      }
      lastScanLogMillis = currentMillis;
    }
  }
  else
  {
    // Reset the timer if we're not scanning
    lastScanLogMillis = currentMillis;
  }

  // Update Bluetooth controller (it will handle BLE initialization internally)
  bluetoothController.update();

  // Initialize comms (BLE) once the audio (A2DP) is initialized and done playing the "Initialized" wav
  if (isDonePlayingInitializationAudio && !isBleInitializationStarted)
  {
    bluetoothController.initializeBLE(isPrimary);
    isBleInitializationStarted = true;
  }

  // Primary Only: If connected to bluetooth speakers and the other skull, check if the Matter controller has been triggered.
  // If so, play a random skit.
  if (isPrimary)
  {
    if (matterTriggerDetected) {
        Serial.printf("MAIN: Matter trigger detected on pin %d\n", MATTER_TRIGGER_PIN);
        matterTriggerDetected = false; // Reset the flag
        
        if (bluetoothController.clientIsConnectedToServer() &&
            bluetoothController.isA2dpConnected() &&
            !audioPlayer->isAudioPlaying() &&
            (millis() - lastTimeAudioPlayed > AUDIO_COOLDOWN_TIME))
        {
            Serial.printf("MAIN: Matter trigger detected (currentMillis: %lu, lastTimeAudioPlayed: %lu). Playing random skit...\n", 
                          currentMillis, lastTimeAudioPlayed);
            lightController.blinkEyes(1);
            ParsedSkit selectedSkit = skitSelector->selectNextSkit();
            String filePath = selectedSkit.audioFile;

            // Attempt to set the characteristic value and verify the result
            // This new process ensures both skulls agree on the audio to be played
            bool success = bluetoothController.setRemoteCharacteristicValue(filePath.c_str());
            if (success)
            {
              std::string finalValue = bluetoothController.getRemoteCharacteristicValue();
              if (finalValue == filePath.c_str())
              {
                Serial.printf("MAIN: Successfully updated BLE characteristic with message: %s\n", filePath.c_str());
                audioPlayer->playNext(filePath);
                lastTimeAudioPlayed = currentMillis; // Update the time immediately when we start playing
              }
              else
              {
                Serial.printf("MAIN: BLE characteristic value mismatch. Expected: %s, Actual: %s\n", filePath.c_str(), finalValue.c_str());
                // Do not play audio if there's a mismatch
              }
            }
            else
            {
              Serial.printf("MAIN: Failed to update BLE characteristic with message: %s\n", filePath.c_str());
            }
        }
    }
  }

  // Check if it's time to move the jaw for breathing
  if (currentMillis - lastJawMovementTime >= BREATHING_INTERVAL && !isAudioPlaying)
  {
    breathingJawMovement();
    lastJawMovementTime = currentMillis;
  }

  // Delay to allow for other componeents to update.
  delay(1);
}