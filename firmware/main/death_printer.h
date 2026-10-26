#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "death_printer_format.h"

namespace skull::printer {

// The CSN-A1X connection is dedicated to UART1: TX GPIO18, RX GPIO19, 9600 8N1 by default.
constexpr size_t kMaxFortuneBytes = 1024;

struct Status {
    bool ready;
    bool busy;
    uint8_t queued;
    uint32_t completed_jobs;
    uint32_t failed_jobs;
    uint32_t cancelled_jobs;
    esp_err_t last_error;
};

// Installs UART and starts the printer worker. Safe to call once at boot.
esp_err_t start(uint32_t baud = 9600);

// Copies a bounded, NUL-terminated ASCII fortune into the two-entry queue.
// Returns false if not started, input is invalid/too long, or the queue is full.
bool enqueue(const char *fortune);
inline bool queue_fortune(const char *fortune) { return enqueue(fortune); }

// Cancels the active fortune and discards queued fortunes.
void cancel();

Status status();
inline bool busy() { const Status value = status(); return value.busy || value.queued != 0; }
inline bool failed() { return status().failed_jobs != 0; }

} // namespace skull::printer
