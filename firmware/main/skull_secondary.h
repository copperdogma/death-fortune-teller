#pragma once
#include <cstdint>
#include "esp_err.h"
#include "esp_gap_ble_api.h"
namespace skull::secondary {
struct Request { uint32_t token; char path[241]; };
// Standalone secondary only: BTDM/Bluedroid must already be initialized. Does
// not replace GAP ownership; the radio dispatcher forwards gap_event().
esp_err_t init();
void gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
// All other APIs belong to the same diagnostic/control task. No new task.
void poll();
bool ready();
void advertising(bool enabled);
bool connected();
bool take_connected();
void busy(bool value);
bool take_request(Request &request);
// Publishes readback and queues legacy notification before granting start.
// False on stale/expired token, disconnect, overflow, or failed notification.
bool respond(uint32_t token, bool accept);
}
