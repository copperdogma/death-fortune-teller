/*
 * ESP32-S3 UART Master - Matter POC Director
 * 
 * This is the MASTER board in a Matter master/slave POC architecture.
 * 
 * Role: Application logic, skit control, CLI for testing
 * Hardware: ESP32-S3-WROOM
 * UART: TX=GPIO17, RX=GPIO18, 115200 baud, 8N1, no flow control
 * 
 * Key Features:
 * - CLI for interactive testing (hello, ping, trigger, mode, status)
 * - Bidirectional UART with CRC8 error detection
 * - Receives commands from Matter node (C3) via UART
 * - Sends responses back to C3
 * - LED feedback for visual confirmation
 * 
 * UART Protocol:
 * Frame: 0xA5 LEN CMD PAYLOAD... CRC8
 * Commands: HELLO(0x01), SET_MODE(0x02), TRIGGER(0x03), PING(0x04)
 * Responses: ACK(0x80), ERR(0x81), BUSY(0x82), DONE(0x83)
 * 
 * Critical Pattern: Always send ACK BEFORE performing slow operations!
 * 
 * See POC-SUMMARY.md for complete documentation.
 */

#include <HardwareSerial.h>

trigger=closed
big kid=trigger
little kid=little kid
closed=big kid
take one= take one

// ===== UART Configuration =====
#define UART_TX_PIN 17
#define UART_RX_PIN 18
#define UART_BAUD 115200
HardwareSerial UartNode(1);  // Use Serial1

// ===== LED Configuration =====
#define LED_BUILTIN 2  // Built-in LED on most ESP32-S3 boards

// ===== Protocol Definitions =====
#define FRAME_START 0xA5
#define CRC_POLY 0x31  // Dallas/Maxim

// Commands (S3 → C3)
#define CMD_HELLO    0x01
#define CMD_SET_MODE 0x02
#define CMD_TRIGGER  0x03
#define CMD_PING     0x04

// Status notifications (C3 → S3)
#define CMD_STATUS_PAIRED    0x10
#define CMD_STATUS_UNPAIRED  0x11

// Responses (C3 → S3)
#define RSP_ACK      0x80
#define RSP_ERR      0x81
#define RSP_BUSY     0x82
#define RSP_DONE     0x83

// ===== Statistics =====
struct {
  uint32_t frames_sent;
  uint32_t frames_received;
  uint32_t ack_count;
  uint32_t err_count;
  uint32_t busy_count;
  uint32_t done_count;
  uint32_t crc_errors;
  uint32_t timeout_count;
} stats;

// ===== CRC8 Calculation =====
uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ CRC_POLY;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// ===== LED Helper Functions =====
void ledBlink(int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(ms);
    digitalWrite(LED_BUILTIN, LOW);
    if (i < times - 1) {
      delay(ms);
    }
  }
}

// ===== Frame Builder =====
// Builds a frame: 0xA5 LEN CMD [PAYLOAD...] CRC8
bool sendFrame(uint8_t cmd, const uint8_t *payload = nullptr, uint8_t payload_len = 0) {
  uint8_t frame[64];  // Max frame size
  uint8_t idx = 0;
  
  // Header
  frame[idx++] = FRAME_START;
  
  // Length = CMD (1 byte) + PAYLOAD
  uint8_t len = 1 + payload_len;
  frame[idx++] = len;
  
  // Command
  frame[idx++] = cmd;
  
  // Payload
  if (payload && payload_len > 0) {
    memcpy(&frame[idx], payload, payload_len);
    idx += payload_len;
  }
  
  // CRC (over LEN + CMD + PAYLOAD)
  uint8_t crc = crc8(&frame[1], idx - 1);
  frame[idx++] = crc;
  
  // Send frame
  size_t written = UartNode.write(frame, idx);
  
  if (written == idx) {
    stats.frames_sent++;
    
    // Debug output
    Serial.print("→ TX: ");
    for (uint8_t i = 0; i < idx; i++) {
      Serial.printf("%02X ", frame[i]);
    }
    Serial.println();
    
    return true;
  }
  
  return false;
}

// ===== Frame Parser =====
// Reads and validates incoming response frames
bool receiveFrame(uint8_t &response_cmd, uint8_t *payload, uint8_t &payload_len, uint32_t timeout_ms = 1000) {
  uint32_t start = millis();
  uint8_t state = 0;  // 0=wait start, 1=len, 2=cmd, 3=payload, 4=crc
  uint8_t frame_len = 0;
  uint8_t bytes_read = 0;
  uint8_t frame_buf[64];
  uint8_t buf_idx = 0;
  
  while (millis() - start < timeout_ms) {
    if (UartNode.available()) {
      uint8_t b = UartNode.read();
      
      switch (state) {
        case 0:  // Wait for start byte
          if (b == FRAME_START) {
            frame_buf[buf_idx++] = b;
            state = 1;
          }
          break;
          
        case 1:  // Length byte
          frame_buf[buf_idx++] = b;
          frame_len = b;
          if (frame_len == 0 || frame_len > 60) {
            // Invalid length, reset
            state = 0;
            buf_idx = 0;
          } else {
            state = 2;
          }
          break;
          
        case 2:  // Command byte
          frame_buf[buf_idx++] = b;
          response_cmd = b;
          bytes_read = 1;  // We've read the CMD byte
          if (bytes_read >= frame_len) {
            state = 4;  // No payload, go to CRC
          } else {
            state = 3;  // Read payload
          }
          break;
          
        case 3:  // Payload bytes
          frame_buf[buf_idx++] = b;
          if (payload) {
            payload[bytes_read - 1] = b;
          }
          bytes_read++;
          if (bytes_read >= frame_len) {
            state = 4;  // Go to CRC
          }
          break;
          
        case 4:  // CRC byte
          frame_buf[buf_idx++] = b;
          uint8_t received_crc = b;
          
          // Calculate CRC over LEN + CMD + PAYLOAD
          uint8_t calc_crc = crc8(&frame_buf[1], buf_idx - 2);
          
          if (calc_crc == received_crc) {
            // Valid frame
            payload_len = frame_len - 1;  // Length includes CMD
            stats.frames_received++;
            
            // Debug output
            Serial.print("← RX: ");
            for (uint8_t i = 0; i < buf_idx; i++) {
              Serial.printf("%02X ", frame_buf[i]);
            }
            Serial.println();
            
            return true;
          } else {
            // CRC error
            stats.crc_errors++;
            Serial.printf("✗ CRC Error: expected %02X, got %02X\n", calc_crc, received_crc);
            state = 0;
            buf_idx = 0;
          }
          break;
      }
    }
    delay(1);  // Small delay to avoid busy loop
  }
  
  // Timeout
  stats.timeout_count++;
  Serial.println("✗ Timeout waiting for response");
  return false;
}

// ===== Command Senders =====
void cmdHello() {
  Serial.println("\n>>> Sending HELLO");
  ledBlink(1, 200);  // Visual feedback
  if (sendFrame(CMD_HELLO)) {
    uint8_t rsp_cmd, rsp_payload[8], rsp_len;
    if (receiveFrame(rsp_cmd, rsp_payload, rsp_len)) {
      handleResponse(rsp_cmd, rsp_payload, rsp_len);
    }
  }
}

void cmdPing() {
  Serial.println("\n>>> Sending PING");
  ledBlink(2, 100);  // Two quick blinks
  if (sendFrame(CMD_PING)) {
    uint8_t rsp_cmd, rsp_payload[8], rsp_len;
    if (receiveFrame(rsp_cmd, rsp_payload, rsp_len)) {
      handleResponse(rsp_cmd, rsp_payload, rsp_len);
    }
  }
}

void cmdTrigger() {
  Serial.println("\n>>> Sending TRIGGER");
  // One long blink
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  
  if (sendFrame(CMD_TRIGGER)) {
    uint8_t rsp_cmd, rsp_payload[8], rsp_len;
    if (receiveFrame(rsp_cmd, rsp_payload, rsp_len, 2000)) {  // Longer timeout
      handleResponse(rsp_cmd, rsp_payload, rsp_len);
    }
  }
}

void cmdSetMode(uint8_t mode) {
  Serial.printf("\n>>> Sending SET_MODE: %d\n", mode);
  if (mode > 3) {
    Serial.println("✗ Invalid mode (0-3)");
    return;
  }
  
  ledBlink(mode + 1, 150);  // Blink (mode+1) times
  
  uint8_t payload[1] = { mode };
  if (sendFrame(CMD_SET_MODE, payload, 1)) {
    uint8_t rsp_cmd, rsp_payload[8], rsp_len;
    if (receiveFrame(rsp_cmd, rsp_payload, rsp_len)) {
      handleResponse(rsp_cmd, rsp_payload, rsp_len);
    }
  }
}

// ===== Response Handler =====
void handleResponse(uint8_t cmd, const uint8_t *payload, uint8_t payload_len) {
  switch (cmd) {
    case RSP_ACK:
      stats.ack_count++;
      Serial.println("✓ ACK received");
      break;
      
    case RSP_ERR:
      stats.err_count++;
      Serial.print("✗ ERR received");
      if (payload_len > 0) {
        Serial.printf(" (code: 0x%02X)", payload[0]);
      }
      Serial.println();
      break;
      
    case RSP_BUSY:
      stats.busy_count++;
      Serial.println("⏳ BUSY received");
      break;
      
    case RSP_DONE:
      stats.done_count++;
      Serial.println("✓ DONE received");
      break;
      
    default:
      Serial.printf("? Unknown response: 0x%02X\n", cmd);
      break;
  }
}

// ===== Statistics Display =====
void showStats() {
  Serial.println("\n=== UART Statistics ===");
  Serial.printf("Frames sent:     %u\n", stats.frames_sent);
  Serial.printf("Frames received: %u\n", stats.frames_received);
  Serial.printf("ACK count:       %u\n", stats.ack_count);
  Serial.printf("ERR count:       %u\n", stats.err_count);
  Serial.printf("BUSY count:      %u\n", stats.busy_count);
  Serial.printf("DONE count:      %u\n", stats.done_count);
  Serial.printf("CRC errors:      %u\n", stats.crc_errors);
  Serial.printf("Timeouts:        %u\n", stats.timeout_count);
  Serial.println("=======================\n");
}

// ===== CLI Parser =====
void processCLI(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  
  if (cmd == "hello") {
    cmdHello();
  }
  else if (cmd == "ping") {
    cmdPing();
  }
  else if (cmd == "trigger") {
    cmdTrigger();
  }
  else if (cmd.startsWith("mode ")) {
    int mode = cmd.substring(5).toInt();
    cmdSetMode((uint8_t)mode);
  }
  else if (cmd == "status" || cmd == "stats") {
    showStats();
  }
  else if (cmd == "help" || cmd == "?") {
    printHelp();
  }
  else if (cmd.length() > 0) {
    Serial.println("✗ Unknown command. Type 'help' for commands.");
  }
}

void printHelp() {
  Serial.println("\n=== CLI Commands ===");
  Serial.println("hello       - Send HELLO handshake");
  Serial.println("ping        - Send PING health check");
  Serial.println("trigger     - Send TRIGGER to start skit");
  Serial.println("mode <0-3>  - Send SET_MODE command");
  Serial.println("status      - Show UART statistics");
  Serial.println("help        - Show this help");
  Serial.println("====================\n");
}

// ===== Setup =====
void setup() {
  // USB Serial for CLI
  Serial.begin(115200);
  delay(100);
  
  // Initialize LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // UART to C3 Node
  UartNode.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 UART Master - Fortune Teller ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("\nUART Config: TX=%d, RX=%d, Baud=%d\n", UART_TX_PIN, UART_RX_PIN, UART_BAUD);
  Serial.printf("LED: GPIO %d initialized\n", LED_BUILTIN);
  Serial.println("\nType 'help' for commands.\n");
  
  // Clear stats
  memset(&stats, 0, sizeof(stats));
}

// ===== Incoming Command Handler =====
// Handles unsolicited commands FROM C3 (triggered by HomeKit)
void checkIncomingCommands() {
  static uint8_t state = 0;  // 0=wait start, 1=len, 2=cmd, 3=payload, 4=crc
  static uint8_t frame_buf[64];
  static uint8_t buf_idx = 0;
  static uint8_t frame_len = 0;
  static uint8_t bytes_read = 0;
  static uint8_t cmd = 0;
  static uint8_t payload[60];
  
  while (UartNode.available()) {
    uint8_t b = UartNode.read();
    
    switch (state) {
      case 0:  // Wait for start byte
        if (b == FRAME_START) {
          frame_buf[buf_idx++] = b;
          state = 1;
        }
        break;
        
      case 1:  // Length byte
        frame_buf[buf_idx++] = b;
        frame_len = b;
        if (frame_len == 0 || frame_len > 60) {
          state = 0;
          buf_idx = 0;
        } else {
          state = 2;
        }
        break;
        
      case 2:  // Command byte
        frame_buf[buf_idx++] = b;
        cmd = b;
        bytes_read = 1;
        if (bytes_read >= frame_len) {
          state = 4;  // No payload
        } else {
          state = 3;
        }
        break;
        
      case 3:  // Payload
        frame_buf[buf_idx++] = b;
        payload[bytes_read - 1] = b;
        bytes_read++;
        if (bytes_read >= frame_len) {
          state = 4;
        }
        break;
        
      case 4:  // CRC
        frame_buf[buf_idx++] = b;
        uint8_t received_crc = b;
        uint8_t calc_crc = crc8(&frame_buf[1], buf_idx - 2);
        
        if (calc_crc == received_crc) {
          // Valid frame - display it
          uint8_t payload_len = frame_len - 1;
          
          Serial.print("\n🔔 INCOMING from C3: ");
          
          // Display based on command type
          if (cmd == CMD_TRIGGER) {
            Serial.println("TRIGGER (HomeKit activated!)");
            ledBlink(1, 500);  // Visual confirmation
          }
          else if (cmd == CMD_SET_MODE && payload_len > 0) {
            const char* mode_names[] = {"Little Kid", "Big Kid", "Take One", "Closed"};
            uint8_t mode = payload[0];
            if (mode <= 3) {
              Serial.printf("SET_MODE %d (%s) (HomeKit brightness changed!)\n", mode, mode_names[mode]);
              ledBlink(mode + 1, 150);
            } else {
              Serial.printf("SET_MODE %d (Invalid!)\n", mode);
            }
          }
          else if (cmd == CMD_STATUS_PAIRED) {
            Serial.println("\n╔═══════════════════════════════════════╗");
            Serial.println("║  🎉 C3 PAIRED WITH HOMEKIT! 🎉       ║");
            Serial.println("║  Device is now controllable via Home  ║");
            Serial.println("╚═══════════════════════════════════════╝\n");
            ledBlink(10, 50);  // Celebration blinks
          }
          else if (cmd == CMD_STATUS_UNPAIRED) {
            Serial.println("\n╔═══════════════════════════════════════╗");
            Serial.println("║  ⚠️  C3 UNPAIRED FROM HOMEKIT         ║");
            Serial.println("║  Scan QR code to re-add device        ║");
            Serial.println("╚═══════════════════════════════════════╝\n");
            ledBlink(3, 200);
          }
          else if (cmd == CMD_HELLO) {
            Serial.println("HELLO");
          }
          else if (cmd == CMD_PING) {
            Serial.println("PING");
          }
          else {
            Serial.printf("Unknown CMD 0x%02X\n", cmd);
          }
          
          // Send ACK back
          sendFrame(RSP_ACK);
        } else {
          Serial.printf("✗ Incoming CRC error\n");
        }
        
        // Reset
        state = 0;
        buf_idx = 0;
        break;
    }
  }
}

// ===== Main Loop =====
void loop() {
  // Check for incoming UART commands from C3 (HomeKit triggers)
  checkIncomingCommands();
  
  // Check for CLI input
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCLI(cmd);
  }
  
  // Small delay
  delay(10);
}
