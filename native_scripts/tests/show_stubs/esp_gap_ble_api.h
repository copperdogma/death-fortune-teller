#pragma once
// Show controller includes the server API but never handles native GAP payloads.
using esp_gap_ble_cb_event_t = int;
union esp_ble_gap_cb_param_t { int unused; };
