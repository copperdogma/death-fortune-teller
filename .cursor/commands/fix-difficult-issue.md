# Fix Difficult ESP32 Issue

Take a step back and analyze the situation and code. Look for root causes.

## Investigate

- Is the code being tested a monolith that's doing way too much that should be refactored?
- Is it violating the Single Responsibility Principle? Is it buggy and untestable?
- Is there something about your approach to testing that keeps failing so you need to try another approach?
- What versions are we using of everything? Are we sure we're adhering to that version's syntax and best practices?
- If you're confident the code being tested is correct, can you add debug statements to the test to see what output we should be expecting?

## ESP32-Specific Investigation

### Hardware Issues
- **Power Supply**: Check if ESP32 is getting stable 3.3V, measure with multimeter
- **Wiring**: Verify all connections, check for loose wires or cold solder joints
- **Components**: Test individual components (servo, thermal printer, SD card) in isolation
- **Ground Loops**: Ensure proper grounding, check for electrical noise

### Firmware Issues
- **Memory**: Check heap/stack usage with `ESP.getFreeHeap()`, watch for memory leaks
- **Timing**: Verify interrupt timing, PWM frequencies, and delay functions
- **Serial Communication**: Check baud rates, buffer sizes, and UART configuration
- **SD Card**: Verify SPI connections, file system integrity, and audio file formats

### PlatformIO Issues
- **Build Configuration**: Check `platformio.ini` for correct board settings and libraries
- **Library Conflicts**: Verify library versions and dependencies
- **Upload Problems**: Check USB connection, boot mode, and upload settings
- **Partition Table**: Ensure OTA partitions are correctly configured

## Do Research

- Use web search for ESP32-specific solutions and known issues
- Check Arduino/ESP-IDF documentation for proper API usage
- Look for similar projects on GitHub for reference implementations
- Search for hardware-specific troubleshooting guides

## Make Hypotheses

Make at least three hypotheses as to what the true reason behind this issue could be, then start checking each hypothesis vs the code to verify or disprove your thesis.

### Common ESP32 Issues to Check

1. **Power Issues**: Insufficient current, voltage drops, brownout conditions
2. **Timing Issues**: Interrupt conflicts, blocking operations, watchdog resets
3. **Memory Issues**: Stack overflow, heap corruption, buffer overruns
4. **Communication Issues**: UART timing, SPI conflicts, I2C bus problems
5. **Hardware Issues**: Component failure, wiring problems, electrical noise

Update [scratchpad.md](mdc:scratchpad.md) with your findings.

Once done, make a recommendation as to how to fix the issue, but don't proceed without permission.

## ESP32 Debugging Tools

- **Serial Monitor**: Use `Serial.println()` for debugging output
- **Memory Monitor**: Check heap usage with `ESP.getFreeHeap()`
- **Core Dump**: Enable core dumps for crash analysis
- **Oscilloscope**: Use for timing analysis and signal integrity
- **Multimeter**: Check voltages and continuity
