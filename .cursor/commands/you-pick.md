# You Pick Next Task

Think about what we need to do to get to our goal.

What's the next most important task on the list to achieve that goal? Think about it.

When you've figured it out, start working on that task.

## ESP32 Project Context

Consider the current state of the Death Fortune Teller project:

### Current System Components
- **ESP32-WROVER**: Main microcontroller with WiFi/Bluetooth
- **SD Card**: Audio file storage and playback
- **Servo Motors**: Jaw movement and skull animation
- **Thermal Printer**: Fortune printing functionality
- **Capacitive Sensors**: Finger detection and interaction
- **UART Communication**: Matter device integration
- **OTA Updates**: Over-the-air firmware updates

### Priority Areas
1. **Core Functionality**: Audio playback, servo control, thermal printing
2. **User Interaction**: Capacitive sensing, snap flow, finger detection
3. **System Integration**: UART communication, Matter device triggers
4. **Reliability**: Error handling, power management, OTA stability
5. **User Experience**: Smooth animations, responsive interactions

### Check Current Status
- Review `/docs/stories.md` for pending tasks
- Check `scratchpad.md` for current work in progress
- Consider what's blocking other features from working
- Prioritize based on user impact and technical dependencies

## Decision Framework

When choosing the next task, consider:

1. **Dependencies**: What needs to be working before other features can be built?
2. **User Impact**: What will make the biggest difference to the user experience?
3. **Technical Risk**: What's most likely to have issues that need debugging?
4. **Testing**: What can be easily tested and validated?
5. **Integration**: What connects multiple system components together?

## Common Next Steps

- **Hardware Issues**: Fix power supply, wiring, or component problems
- **Firmware Bugs**: Debug memory leaks, timing issues, or communication problems
- **Feature Implementation**: Add new functionality or improve existing features
- **Testing & Validation**: Verify system works end-to-end
- **Documentation**: Update specs, stories, or technical documentation
