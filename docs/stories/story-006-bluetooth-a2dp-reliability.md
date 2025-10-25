# Story: Bluetooth A2DP Reliability & Testing

**Status**: To Do

---

## Related Requirement
[docs/spec.md §13 Risks & Mitigations — Audio Reliability](../spec.md#13-risks--mitigations)

## Alignment with Design
[docs/spec.md §4 Assets & File Layout — Audio](../spec.md#4-assets--file-layout)

## Notes
**This story may already be complete!** Audio was successfully playing earlier, and the ISSUE.md file may be out of date. This story is primarily to verify everything is working properly and clean up any outdated documentation.

## Acceptance Criteria
- Bluetooth speaker pairs and reconnects automatically after disconnections, honoring the 5s retry policy
- Audio playback remains glitch-free during concurrent operations (printing, servo motion) for at least a 10-minute endurance run
- Logging captures connection state transitions to aid field diagnostics without flooding the console
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Verify Current Functionality**: Test Bluetooth connection, pairing, and audio playback
- [ ] **Clean Up Documentation**: Remove outdated references to connection state bugs in ISSUE.md
- [ ] **Conduct Soak Testing**: Run 10-minute endurance test with concurrent operations
- [ ] **Test Audio Underruns**: Verify glitch-free audio during printing and servo motion
- [ ] **Document Findings**: Summarize any remaining issues or recommended speaker settings

## Technical Implementation Details

### Verification Testing
```cpp
void verifyBluetoothFunctionality() {
    LOG_INFO(TAG, "🔍 Verifying Bluetooth A2DP functionality...");
    
    // Test connection
    if (bluetoothController->isA2dpConnected()) {
        LOG_INFO(TAG, "✅ Bluetooth connected successfully");
    } else {
        LOG_ERROR(TAG, "❌ Bluetooth not connected");
        return;
    }
    
    // Test audio playback
    audioPlayer->playNext("/audio/test.wav");
    if (audioPlayer->isAudioPlaying()) {
        LOG_INFO(TAG, "✅ Audio playback working");
    } else {
        LOG_ERROR(TAG, "❌ Audio playback failed");
    }
}
```

### Soak Testing
```cpp
void runSoakTest() {
    LOG_INFO(TAG, "🧪 Starting 10-minute soak test...");
    unsigned long startTime = millis();
    unsigned long testDuration = 10 * 60 * 1000; // 10 minutes
    
    while (millis() - startTime < testDuration) {
        // Simulate concurrent operations
        if (millis() % 30000 < 5000) { // Every 30s for 5s
            // Simulate printing
            if (thermalPrinter) {
                thermalPrinter->printFortune("Soak test fortune");
            }
        }
        
        if (millis() % 15000 < 2000) { // Every 15s for 2s
            // Simulate servo motion
            servoController.setPosition(80);
            delay(1000);
            servoController.setPosition(20);
        }
        
        // Check for audio underruns
        if (audioPlayer->hasAudioUnderrun()) {
            LOG_ERROR(TAG, "❌ Audio underrun detected during soak test!");
            break;
        }
        
        delay(100);
    }
    
    LOG_INFO(TAG, "✅ Soak test completed");
}
```

## Notes
- **Status**: Likely already working - just needs verification
- **Documentation**: Clean up outdated ISSUE.md references
- Coordinate test cases with Stories 004 (Finger Detection) and 005 (Thermal Printer)
- Test with actual Bluetooth speaker hardware
- Monitor power consumption during concurrent operations
