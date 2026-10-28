# Continuous speaker reconnect — 2026-10-27

Problem class: asynchronous connection recovery when a remote device becomes
available after a finite discovery window. Use repeated paced attempts and
completion events, not an exhausted lifetime retry budget.

## Sources and applicability

- [ESP-IDF5.5.4 A2DP API](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/bluetooth/esp_a2dp.html): connect/disconnect success means a request was queued; actual connection state is asynchronous.
- [Classic GAP API](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/bluetooth/esp_gap_bt.html): inquiry is finite and reports discovery-state completion. The application owns restarting it.
- Installed pinned SDK, components/bt/host/bluedroid/btc/profile/std/a2dp/btc_av.c:
  btc_av_state_opening_handler has no disconnect-request handler. Therefore
  esp_a2d_source_disconnect is not established as a cancellation of an in-flight
  open. A new connect must wait for the completion callback. API return ESP_OK
  alone cannot prove the previous attempt was cancelled.
- Local native firmware/main/skull_radio.cpp: three cached attempts, then one
  inquiry; no timer rearmed when inquiry ends without a match. Connection/ACL
  timeout latches and media retry ceiling introduce additional permanent stops.

## Decision and local evidence

Repeat episodes forever with5second backoff. Preserve configured-name/address
selection and connection/ACL ownership gates. On slow SDK operations, continue
observing and periodically warn; automatically proceed after their completion.
Do not tear down the shared Matter/Bluetooth host or invent cancellation.

Live USB: uptime339s speaker0/stream0, callbacks0, no radio fault; explicit retry
346866ms connected saved speaker348516ms and stream348626ms, startup delivered
353076ms. This verifies available speaker/cache/transport and existing retry,
not the unavailable earlier boot logs or the new repeated-episode implementation.

Remaining uncertainty: a permanently wedged SDK operation cannot safely be
cancelled with the documented opening-state behavior. This patch removes the
application's arbitrary stop, without claiming recovery from all controller
faults. Actual delayed-speaker power-on requires a physical device experiment.

## Updated implementation validation

Actual production-source host harness passes20 failed episodes and subsequent
connection, uncached discovery, immediate API failures, delayed callback/ACL
release beyond former timeouts and >20 media retries. The harness fails against
the original firmware at the first exhausted episode. ASan/UBSan plus all host
and Death suites and native build pass. App-only upload hash verified. On real
hardware with speaker off, third cached failure31086ms and inquiry ending46366ms
were followed automatically by a new cached episode51386ms and another inquiry
81836ms. No console intervention between cycles.

Speaker powered on later: cached attempt117406ms connected120646ms, stream
ready120716ms, startup delivered125166ms. Cam confirmed audible announcement.
No manual retry/reboot/pairing reset after power-on. This establishes the new
late-availability recovery on actual hardware; it is not a long endurance test.
