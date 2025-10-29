# Story: Access WROVER SD Card From USB-C

**Status**: Future

---

## Related Requirement
User workflow improvement - eliminate need to physically remove SD card for file updates

## Alignment with Design
[docs/spec.md §4 Assets & File Layout](../spec.md#4-assets--file-layout) - SD card file management

## Acceptance Criteria
- [ ] ESP32-WROVER presents SD card as mountable volume on Mac via USB-C
- [ ] Files can be uploaded/downloaded without removing SD card
- [ ] System remains functional during file transfer operations
- [ ] File transfer performance is acceptable for typical audio file sizes
- [ ] User must sign off on functionality before story can be marked complete

## Research Summary

### a) Is it possible?

**ESP32-WROVER Limitations:**
- **No native USB OTG support** - The ESP32-WROVER lacks the hardware required for USB device functionality
- **USB-to-Serial only** - Current USB connection is limited to programming and serial communication
- **Hardware modification required** - Would need external USB-to-serial bridge (like FTDI FT2232HL) and custom firmware

**Alternative ESP32 Variants:**
- **ESP32-S2/S3** - Have native USB OTG capabilities and can implement USB Mass Storage Class (MSC)
- **Proven implementations** - Multiple working examples available for ESP32-S2/S3

### b) How much work is it?

**ESP32-WROVER Implementation:**
- **High complexity** - Requires external hardware integration and custom USB stack development
- **Significant effort** - Estimated 2-3 weeks of development time
- **Unreliable** - May not provide stable USB MSC functionality
- **Not recommended** - Hardware limitations make this approach impractical

**ESP32-S2/S3 Implementation:**
- **Moderate complexity** - Native USB support simplifies development
- **Reasonable effort** - Estimated 1-2 weeks of development time
- **Proven approach** - Well-documented with working examples
- **Recommended** - Most practical solution

### c) High-level overview with code samples

#### Option 1: ESP32-S2/S3 USB MSC Implementation

**Hardware Requirements:**
- ESP32-S2 or ESP32-S3 development board
- SD card connected via SDMMC interface
- USB-C connector for host connection

**Software Implementation:**
```c
#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "sdmmc_cmd.h"

static const char *TAG = "usb_msc_sd";

// Initialize SD card
static esp_err_t init_sd_card(sdmmc_card_t **card) {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    return esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, 
                                   &mount_config, card);
}

// MSC read callback
static int32_t msc_read_cb(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    // Read from SD card at logical block address
    FILE* file = fopen("/sdcard", "rb");
    if (file) {
        fseek(file, lba * 512 + offset, SEEK_SET);
        size_t bytes_read = fread(buffer, 1, bufsize, file);
        fclose(file);
        return bytes_read;
    }
    return 0;
}

// MSC write callback  
static int32_t msc_write_cb(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    // Write to SD card at logical block address
    FILE* file = fopen("/sdcard", "r+b");
    if (file) {
        fseek(file, lba * 512 + offset, SEEK_SET);
        size_t bytes_written = fwrite(buffer, 1, bufsize, file);
        fclose(file);
        return bytes_written;
    }
    return 0;
}

void app_main(void) {
    sdmmc_card_t *card;
    
    // Initialize SD card
    if (init_sd_card(&card) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD card");
        return;
    }
    
    // Initialize TinyUSB
    tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
    };
    tinyusb_driver_install(&tusb_cfg);
    
    // Initialize MSC
    const tinyusb_msc_storage_config_t msc_cfg = {
        .read_cb = msc_read_cb,
        .write_cb = msc_write_cb,
        .block_count = card->csd.capacity,
        .block_size = 512,
    };
    tinyusb_msc_storage_init(&msc_cfg);
    
    ESP_LOGI(TAG, "USB MSC initialized - SD card accessible via USB");
}
```

#### Option 2: WiFi-Based File Transfer (Recommended for ESP32-WROVER)

**Hardware Requirements:**
- Existing ESP32-WROVER setup
- WiFi connectivity
- Web browser on Mac

**Software Implementation:**
```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <SD_MMC.h>
#include <SPIFFS.h>

WebServer server(80);

void setup() {
    Serial.begin(115200);
    
    // Initialize SD card
    if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_20M)) {
        Serial.println("SD Card Mount Failed");
        return;
    }
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    
    Serial.println("WiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/upload", HTTP_POST, handleUpload);
    server.on("/download", handleDownload);
    server.on("/list", handleList);
    
    server.begin();
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>SD Card Manager</title></head><body>";
    html += "<h1>SD Card File Manager</h1>";
    html += "<h2>Upload File</h2>";
    html += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
    html += "<input type='file' name='file'><br><br>";
    html += "<input type='submit' value='Upload'>";
    html += "</form>";
    html += "<h2>Files on SD Card</h2>";
    html += "<a href='/list'>List Files</a>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleUpload() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        String filename = "/sdcard" + upload.filename;
        uploadFile = SD_MMC.open(filename, FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
        }
        server.send(200, "text/plain", "File uploaded successfully");
    }
}

void handleList() {
    String html = "<!DOCTYPE html><html><head><title>SD Card Files</title></head><body>";
    html += "<h1>Files on SD Card</h1><ul>";
    
    File root = SD_MMC.open("/sdcard");
    File file = root.openNextFile();
    
    while (file) {
        html += "<li><a href='/download?file=" + String(file.name()) + "'>" + 
                String(file.name()) + "</a> (" + String(file.size()) + " bytes)</li>";
        file = root.openNextFile();
    }
    
    html += "</ul><a href='/'>Back to Upload</a></body></html>";
    server.send(200, "text/html", html);
}

void loop() {
    server.handleClient();
}
```

## Tasks

### Option 1: ESP32-S2/S3 USB MSC (Hardware Upgrade Required)
- [ ] **Hardware Migration**: Replace ESP32-WROVER with ESP32-S2 or ESP32-S3
- [ ] **Pin Mapping Update**: Update all pin assignments for new board
- [ ] **USB MSC Implementation**: Implement TinyUSB MSC with SD card integration
- [ ] **File System Integration**: Ensure proper FAT32/exFAT support
- [ ] **Testing**: Validate USB MSC functionality on Mac
- [ ] **Performance Optimization**: Tune transfer speeds and reliability

### Option 2: WiFi File Transfer (Current Hardware)
- [ ] **Web Server Setup**: Implement HTTP server for file management
- [ ] **File Upload Handler**: Create multipart form handling for file uploads
- [ ] **File Download Handler**: Implement file serving for downloads
- [ ] **Directory Listing**: Add file browser functionality
- [ ] **Authentication**: Add basic security for file access
- [ ] **Mobile Interface**: Optimize web interface for mobile devices

### Option 3: Serial File Transfer Protocol (Fallback)
- [ ] **Protocol Design**: Define custom serial protocol for file transfer
- [ ] **Mac Client**: Develop desktop application for file management
- [ ] **ESP32 Server**: Implement file transfer server on ESP32
- [ ] **Progress Indication**: Add transfer progress and error handling
- [ ] **File Validation**: Implement checksums for data integrity

## Technical Considerations

### USB MSC Implementation Challenges
- **Hardware Requirements**: ESP32-WROVER lacks native USB OTG
- **Power Management**: USB MSC may interfere with other system functions
- **File System Locking**: Need to handle concurrent access between USB and internal operations
- **Performance**: USB 1.1 full-speed limited to ~12 Mbps

### WiFi File Transfer Benefits
- **No Hardware Changes**: Works with existing ESP32-WROVER
- **Cross-Platform**: Works on any device with web browser
- **Concurrent Access**: Multiple users can access files simultaneously
- **Remote Management**: Can manage files from anywhere on network

### Performance Expectations
- **USB MSC**: 5-10 MB/s transfer rate
- **WiFi Transfer**: 1-5 MB/s depending on signal strength
- **File Size Limits**: Both methods support large audio files (10+ MB)

## Recommended Approach

**Phase 1: WiFi File Transfer (Immediate)**
- Implement web-based file management using existing hardware
- Provides immediate solution without hardware changes
- Can be implemented in 1-2 weeks

**Phase 2: Hardware Upgrade (Future)**
- Consider ESP32-S3 upgrade for native USB MSC support
- Provides native USB mass storage experience
- Requires hardware redesign and testing

## Dependencies
- WiFi connectivity for Option 2
- USB-C connection for Option 1
- File system compatibility (FAT32/exFAT)

## Notes
- USB MSC requires ESP32-S2/S3 with native USB OTG support
- WiFi solution works with current ESP32-WROVER hardware
- Consider security implications for WiFi file access
- File transfer may impact audio playback performance during transfers
- Both solutions require careful file system management to prevent corruption

## References
- [ESP32-S2 USB Device Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/usb_device.html)
- [TinyUSB MSC Examples](https://github.com/hathach/tinyusb/tree/master/examples)
- [ESP32 Arduino WebServer Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer)
- [ESP32 SD_MMC Library Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/sd_mmc.html)
