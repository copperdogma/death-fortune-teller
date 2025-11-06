# Story 014 Keepers — Files to Preserve Before Revert

This list captures code/assets worth retaining from the Story 012/013 work so we can cherry‑pick into the new, lighter modular runtime (Story 014). Use `scripts/collect_keepers.sh` to archive them before performing any revert.

## Code Keepers

Core CLI components (always-on across props):
- src/cli_command_router.h
- src/cli_command_router.cpp
- src/cli_service.h
- src/cli_service.cpp

Content selection + pure helpers (host-testable and small):
- src/audio_directory_selector.h
- src/audio_directory_selector.cpp
- src/skit_selector.h
- src/skit_selector.cpp
- src/infra/circular_audio_buffer.h

Unit tests to keep (valuable, lightweight):
- tests/unit/test_cli_command_router/test_main.cpp
- tests/unit/test_audio_directory_selector/test_main.cpp
- tests/unit/test_skit_selector/test_main.cpp
- tests/unit/test_circular_audio_buffer/test_main.cpp

Minimal UNIT_TEST stubs used by the above suites:
- tests/support/finger_sensor_stub.h
- tests/support/servo_controller_stub.h
- tests/support/thermal_printer_stub.h
- tests/support/Print.h
- tests/support/Servo.h

## Story Snapshots (for context only)
- docs/stories/story-012-runtime-modularity-and-coverage.md
- docs/stories/story-013-runtime-diagnostics-and-coverage.md

## Notes
- Heavy DI/service layers (runtime_interfaces, runtime_adapters, runtime_loop_services, etc.) are excluded from the baseline for Story 014; keep only as reference if desired.
- After archiving, we will revert 012/013 story docs and the related code changes per discussion.

