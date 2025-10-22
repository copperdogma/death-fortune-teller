# Changelog

20251021: Project Created.

20251022: Codex got the base code working in platformio! It was based off TwoSkulls, but that was build in Arduino IDE and I wasnt sure if that was REQUIRED to get the code working, especailly the esp32-a2dp library and audio streaming. But Codex got it working under platformio.

20251022: Optimized Bluetooth reconnect flow — pairing now consistently completes in ~3 seconds with audio playback kicking off immediately after connect.

20251022: Reinstated jaw animation pipeline for verification; startup temporarily plays “Skit - imitations.wav” so the servo can be observed syncing with live audio frames.
