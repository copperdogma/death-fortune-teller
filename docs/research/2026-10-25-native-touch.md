# Native capacitive sensor migration

Problem class: periodic sensor acquisition must not block the shared audio/animation control task, and capacitive readings drift/noise over time.

Sources inspected:
- [Espressif ESP32 touch API](https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32/api-reference/peripherals/touch_pad.html): filtered/raw cached access requires the SDK filter first; classic ESP32 capacitance lowers counts.
- Installed ESP-IDF 5.5.4 legacy `driver/touch_pad.h` API is the build authority for this port. The legacy path matches the ESP32 hardware and existing Arduino measurement-cycle settings.
- Original `src/finger_sensor.cpp` supplies local behavior: one-second baseline sampling, two-second settle interval, adaptive noise threshold, normalized drop, stable detection and slow untouched drift.

Decision: configure GPIO4 / TOUCH_PAD_NUM0 with the legacy timer FSM and SDK filter; read cached raw data every 20ms, then perform Death's own exponential smoothing and normalized threshold outside Bluetooth callbacks. Preserve configurable measurement cycles, smoothing/drift/sensitivity and stable-duration fields. Zero/failed samples clear detection. Baseline updates only while idle/waiting-near and untouched; never adapt to a finger during the timed interaction. Calibration can be explicitly restarted by the console adapter.

Local evidence: production `death_touch.cpp` compiled with warnings-as-errors and address/undefined sanitizers against a cache-only driver stub. Scenarios cover calibration/settling, stable touch, frozen baseline during interaction, invalid samples, removal and recalibration. `death_show.cpp` source tests exercise far/near gates, touch/snap, no-finger timeout, print-once plus transport wait, cancellation/no replay, missing assets, prebuffer timeout and startup/busy gates. `death_fortune.cpp` tests use the actual SDK cJSON source and cover consistent repeated tokens, rejected empty/missing wordlists, malformed templates, empty templates, newline literals, malformed/trailing JSON and stale-load reset.

Uncertainty: these are host tests, not electrical sensor acceptance. The SDK timer/filter introduces its own smoothing/scheduler behavior and old Arduino's synchronous multisamples are replaced by a configurable rolling history of up to32 asynchronous20ms measurements (default32). This adds up to640ms of history and requires physical response-time tuning. The original0.1% noise floor and5% threshold ceiling are preserved. Exact noise behavior and GPIO4 coexistence with the actual onboard SD slot wiring need bench verification. The existing capacitive shield/wiring problems are not repaired by software.
