# Fault log retention after intermittent SD read failures

Cam asks whether system errors should dump recent logs to SD for later inspection. Yes, but the failed SD path cannot be the sole evidence destination.

## Bounded research and design decision

General problem: retaining fault provenance when the logging destination itself is unreliable, without blocking real-time audio or recursively logging logger failures.

Primary sources inspected:
- ESP-IDF5.5.4 logging: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/system/log.html . Custom output callbacks require concurrent-call safety; capture must be bounded and must not perform filesystem/NVS writes in the log hook or audio callback.
- Installed components/nvs_flash/include/nvs.h and existing skull_radio.cpp demonstrate small nvs_set_blob/nvs_commit records in a separate namespace. Use a bounded versioned fault record, not unbounded text or frequent writes; preserve Matter and speaker namespaces. Check actual free NVS entries before implementation and report persistence failures without erasing NVS.
- Core dump guide: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/core_dump.html . Panic dumps require a reserved flash partition. Current factory partition occupies through4MiB; do not claim flash core dumps enabled or change partitioning as part of ordinary logging.

Recommended design: bounded recent-event RAM ring; freeze a fault snapshot with build ID, uptime/reset reason, show phase/asset, SD error, Bluetooth state and heap/underrun counters. After playback cleanup, attempt a rate-limited compact internal-flash fault record independent of SD. Export a capped set of reports to SD only when that path is usable, plus a serial retrieval command. Avoid automatic repeated writes to a card currently returning CRC errors. Redact provisioning/config secrets and network credentials. Distinguish ordinary faults from panics and abrupt power loss; RAM alone cannot survive reset, nor can a pending persistence write be guaranteed on power removal.

This is a design recommendation, not an implemented or flashed logging system. Existing uninstalled candidate retains only the last show interruption in RAM; it is not a rolling log, reboot-persistent report or crash dump. The immediate cutoff mechanism is already captured in the private serial log and sanitized validation report. Remaining uncertainty: resource budget, free NVS capacity, flash-write latency and safe export/recovery behavior need bounded tests before deployment.
