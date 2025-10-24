// ----- config -----
const int TOUCH_PIN = T2;   // GPIO2 (ESP32-S3 WROOM)

// Tunable parameters
float THRESH_N = 0.002f;        // 0.2% change threshold
int   DEBOUNCE = 30;           // ms
int   PRINT_INTERVAL = 500;    // ms - how often to print status
bool  CONTINUOUS_PRINT = true; // toggle serial output

float base=0, filt=0;
uint32_t lastPrint = 0;
String cmdBuffer = "";

void setup(){
  // pinMode(LED_PIN, OUTPUT);  // LED disabled for this POC
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32-S3 Finger Detector Starting ===");

  // ESP32-S3 touch sensor configuration
  // Lower sensitivity to prevent saturation with external electrodes
  touchSetCycles(0x1000, 0x1000);   // Moderate sensitivity for S3
  
  Serial.println("Calibrating baseline (1 second)...");
  delay(500); // Let sensor stabilize
  
  // 1s baseline after everything is still/open
  for(int i=0;i<100;i++){ 
    base = 0.9*base + 0.1*touchRead(TOUCH_PIN); 
    delay(10); 
  }
  filt = base;
  Serial.print("Baseline value: ");
  Serial.println(base);
  
  // Check if sensor is saturated
  if (base > 4000000) {
    Serial.println("WARNING: Sensor appears saturated! Try:");
    Serial.println("  - Reducing electrode size");
    Serial.println("  - Adding a small resistor (1M ohm) in series");
    Serial.println("  - Moving away from the sensor during calibration");
  }
  
  Serial.println("Ready!");
  Serial.println("Type 'help' for commands.\n");
}

// LED flash disabled for POC
// void flash3(){ for(int i=0;i<3;i++){ digitalWrite(LED_PIN,1); delay(70); digitalWrite(LED_PIN,0); delay(70);} }

void calibrateBaseline() {
  Serial.println("\n>>> Calibrating baseline (1 second)...");
  Serial.println(">>> REMOVE YOUR FINGER NOW!");
  delay(1000);
  
  base = 0;
  for(int i=0;i<100;i++){ 
    base = 0.9*base + 0.1*touchRead(TOUCH_PIN); 
    delay(10); 
  }
  filt = base;
  
  Serial.print(">>> New baseline: ");
  Serial.println(base);
  
  if (base > 4000000) {
    Serial.println(">>> WARNING: Sensor appears saturated!");
  } else {
    Serial.println(">>> Calibration complete!\n");
  }
}

void printHelp() {
  Serial.println("\n=== TOUCH SENSOR CLI ===");
  Serial.println("Commands:");
  Serial.println("  cal            - Recalibrate baseline");
  Serial.println("  thresh <val>   - Set threshold (0.0-1.0, e.g. 0.03 = 3%)");
  Serial.println("  debounce <ms>  - Set debounce time (ms, e.g. 30)");
  Serial.println("  interval <ms>  - Set print interval (ms, e.g. 500)");
  Serial.println("  print <on|off> - Toggle continuous printing");
  Serial.println("  status         - Show current values");
  Serial.println("  settings       - Show all settings");
  Serial.println("  help           - Show this help");
  Serial.println();
}

void printSettings() {
  Serial.println("\n=== CURRENT SETTINGS ===");
  Serial.print("Threshold:       "); Serial.print(THRESH_N * 100, 1); Serial.println("%");
  Serial.print("Debounce:        "); Serial.print(DEBOUNCE); Serial.println(" ms");
  Serial.print("Print Interval:  "); Serial.print(PRINT_INTERVAL); Serial.println(" ms");
  Serial.print("Continuous:      "); Serial.println(CONTINUOUS_PRINT ? "ON" : "OFF");
  Serial.print("Baseline:        "); Serial.println(base, 0);
  Serial.println();
}

void printStatus() {
  int raw = touchRead(TOUCH_PIN);
  float d = (filt - base) / (base > 1 ? base : 1);
  
  Serial.println("\n=== CURRENT STATUS ===");
  Serial.print("Raw value:       "); Serial.println(raw);
  Serial.print("Filtered:        "); Serial.println(filt, 0);
  Serial.print("Baseline:        "); Serial.println(base, 0);
  Serial.print("Delta:           "); Serial.print(d * 100, 1); Serial.println("%");
  Serial.print("Detection:       ");
  
  bool detected = d > THRESH_N;
  
  if (detected) {
    Serial.println("YES - Threshold exceeded");
  } else {
    Serial.println("NO");
  }
  Serial.println();
}

void processCLI(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  
  if (cmd == "help" || cmd == "?") {
    printHelp();
  }
  else if (cmd == "cal" || cmd == "calibrate") {
    calibrateBaseline();
  }
  else if (cmd == "settings") {
    printSettings();
  }
  else if (cmd == "status") {
    printStatus();
  }
  else if (cmd.startsWith("thresh ")) {
    float val = cmd.substring(7).toFloat();
    if (val > 0 && val <= 1.0) {
      THRESH_N = val;
      Serial.print(">>> Threshold set to "); Serial.print(THRESH_N * 100, 1); Serial.println("%\n");
    } else {
      Serial.println(">>> ERROR: Value must be 0.0-1.0 (e.g. 0.03)\n");
    }
  }
  else if (cmd.startsWith("debounce ")) {
    int val = cmd.substring(9).toInt();
    if (val >= 0 && val <= 5000) {
      DEBOUNCE = val;
      Serial.print(">>> Debounce set to "); Serial.print(DEBOUNCE); Serial.println(" ms\n");
    } else {
      Serial.println(">>> ERROR: Invalid value\n");
    }
  }
  else if (cmd.startsWith("interval ")) {
    int val = cmd.substring(9).toInt();
    if (val >= 100 && val <= 10000) {
      PRINT_INTERVAL = val;
      Serial.print(">>> Print interval set to "); Serial.print(PRINT_INTERVAL); Serial.println(" ms\n");
    } else {
      Serial.println(">>> ERROR: Value must be 100-10000 ms\n");
    }
  }
  else if (cmd.startsWith("print ")) {
    String arg = cmd.substring(6);
    arg.trim();
    if (arg == "on" || arg == "1") {
      CONTINUOUS_PRINT = true;
      Serial.println(">>> Continuous printing ON\n");
    } else if (arg == "off" || arg == "0") {
      CONTINUOUS_PRINT = false;
      Serial.println(">>> Continuous printing OFF\n");
    } else {
      Serial.println(">>> ERROR: Use 'print on' or 'print off'\n");
    }
  }
  else if (cmd.length() > 0) {
    Serial.println(">>> Unknown command. Type 'help' for commands.\n");
  }
}

void loop(){
  static uint32_t okSince=0;

  // Handle serial commands
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdBuffer.length() > 0) {
        processCLI(cmdBuffer);
        cmdBuffer = "";
      }
    } else {
      cmdBuffer += c;
    }
  }

  int raw = touchRead(TOUCH_PIN);           // lower = closer on ESP32, higher = closer on S3
  
  // Detect saturation and warn
  if (raw > 4000000) {
    Serial.println("!!! SENSOR SATURATED - Restart required or reduce sensitivity !!!");
    delay(1000);
    return;
  }
  
  filt = 0.7f*filt + 0.3f*raw;
  
  // Slow baseline drift compensation (adapts to temperature/humidity changes)
  base = 0.9999f*base + 0.0001f*filt;
  
  // ESP32-S3: values INCREASE when touched (opposite of original ESP32)
  float d = (filt - base) / (base > 1 ? base : 1);

  bool detected = d > THRESH_N;  // normalized increase detection

  uint32_t now = millis();
  
  // Print status every PRINT_INTERVAL ms (if continuous mode enabled)
  if (CONTINUOUS_PRINT && now - lastPrint >= PRINT_INTERVAL) {
    Serial.print("Touch: ");
    Serial.print(raw);
    Serial.print(" | filt: ");
    Serial.print(filt, 0);
    Serial.print(" | base: ");
    Serial.print(base, 0);
    Serial.print(" | Δ: ");
    Serial.print(d * 100, 1);
    Serial.print("%");
    
    if (detected) {
      Serial.print(" <<< DETECTED");
    }
    Serial.println();
    lastPrint = now;
  }
  
  if (detected){
    if (!okSince) okSince = now;
    if (now - okSince >= DEBOUNCE){ 
      Serial.println("\n*** CONFIRMED TOUCH! ***\n");
      okSince=0; 
      delay(300); 
    }
  } else okSince = 0;

  delay(10); // ~100 Hz
}