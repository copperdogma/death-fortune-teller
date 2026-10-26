#include "death_printer.h"
#include "death_printer_logo.h"

#include <atomic>
#include <cstring>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace skull::printer {
namespace {
constexpr char kTag[] = "death_printer";
constexpr uart_port_t kPort = UART_NUM_1;
constexpr int kTxPin = 18;
constexpr int kRxPin = 19;
constexpr size_t kQueueDepth = 2;
constexpr size_t kColumns = 32;
constexpr TickType_t kLinePace = pdMS_TO_TICKS(25);
// Receiver processing time is separate from UART wire completion. This is a
// conservative controlled pacing experiment, not a verified BUSY handshake.
constexpr TickType_t kLogoRowPace = pdMS_TO_TICKS(50);
constexpr size_t kMaxWriteBytes = 64;

struct Job { uint32_t cancel_epoch; char text[kMaxFortuneBytes + 1]; };
QueueHandle_t queue_handle;
TaskHandle_t worker_handle;
std::atomic<bool> started{false};
std::atomic<uint32_t> cancel_epoch{0};
std::atomic<uint32_t> outstanding_jobs{0};
uint32_t printer_baud = 9600;
portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
Status current{false, false, 0, 0, 0, 0, ESP_OK};

void set_busy(bool busy) {
    portENTER_CRITICAL(&state_lock);
    current.busy = busy;
    portEXIT_CRITICAL(&state_lock);
}

void note_result(esp_err_t error, bool cancelled) {
    portENTER_CRITICAL(&state_lock);
    current.busy = false;
    if (cancelled) ++current.cancelled_jobs;
    else if (error == ESP_OK) ++current.completed_jobs;
    else {
        ++current.failed_jobs;
        if (current.ready) current.last_error = error;
        current.ready = false;
    }
    portEXIT_CRITICAL(&state_lock);
}

bool is_ready() {
    portENTER_CRITICAL(&state_lock);
    const bool ready = current.ready;
    portEXIT_CRITICAL(&state_lock);
    return ready;
}

bool was_cancelled(uint32_t epoch) {
    return cancel_epoch.load(std::memory_order_relaxed) != epoch;
}

bool write_bytes(const uint8_t *bytes, size_t length, uint32_t epoch) {
    if (length > kMaxWriteBytes || was_cancelled(epoch)) return false;
    [[maybe_unused]] const int64_t started_us = esp_timer_get_time();
    const int count = uart_write_bytes(kPort, bytes, length);
    if (count != static_cast<int>(length)) {
        ESP_LOGE(kTag, "UART write failed: wanted=%u accepted=%d elapsed_us=%lld cancelled=%d",
                 static_cast<unsigned>(length), count,
                 static_cast<long long>(esp_timer_get_time() - started_us), was_cancelled(epoch));
        return false;
    }
    // Ten serial bits per byte (8N1), plus a fixed 100ms driver margin. Since
    // every write is capped at 64 bytes and the prior write is drained first,
    // this bounds the only wait on the UART task.
    const uint32_t wire_ms = static_cast<uint32_t>((length * 10000ULL + printer_baud - 1) / printer_baud);
    const TickType_t timeout = pdMS_TO_TICKS(wire_ms + 100);
    const esp_err_t drained = uart_wait_tx_done(kPort, timeout);
    if (drained != ESP_OK) {
        ESP_LOGE(kTag, "UART drain failed: bytes=%u result=%s timeout_ticks=%u elapsed_us=%lld cancelled=%d",
                 static_cast<unsigned>(length), esp_err_to_name(drained), static_cast<unsigned>(timeout),
                 static_cast<long long>(esp_timer_get_time() - started_us), was_cancelled(epoch));
        return false;
    }
    return !was_cancelled(epoch);
}

bool feed_blank_line(uint32_t epoch) {
    // ESC J advances a physical30-dot line even with an empty text buffer.
    static constexpr uint8_t feed[] = {0x1b, 'J', 30};
    if (!write_bytes(feed, sizeof(feed), epoch)) return false;
    // Paper movement is not complete just because its3-byte command drained.
    // Keep each wait quantum short so Dead Stop can stop subsequent commands.
    for (unsigned quantum = 0; quantum < 2; ++quantum) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (was_cancelled(epoch)) return false;
    }
    return true;
}

bool write_line(const char *bytes, size_t length, uint32_t epoch) {
    if (length == 0) return feed_blank_line(epoch);
    uint8_t output[kColumns + 1];
    if (length > kColumns) return false;
    if (length) std::memcpy(output, bytes, length);
    output[length] = '\n';
    if (!write_bytes(output, length + 1, epoch)) return false;
    // Line pacing is additional to, and follows, complete UART wire drain.
    vTaskDelay(kLinePace);
    return !was_cancelled(epoch);
}

bool print_logo(uint32_t epoch) {
    // The vendor GS v0 command prints/feeds its own exact row height. Using
    // one row per complete packet bounds cancellation and preserves framing;
    // a partial multi-row raster cannot be abandoned safely mid-payload.
    uint8_t packet[detail::logo::packet_bytes];
    static_assert(sizeof(packet) <= kMaxWriteBytes);
    for (unsigned row = 0; row < detail::logo::height; ++row) {
        if (!detail::logo::row_packet(row, packet) || !write_bytes(packet, sizeof(packet), epoch)) {
            ESP_LOGI(kTag, "Logo transport stopped at row=%u/%u cancelled=%d", row, detail::logo::height, was_cancelled(epoch));
            return false;
        }
        vTaskDelay(kLogoRowPace);
        if (was_cancelled(epoch)) return false;
    }
    return !was_cancelled(epoch);
}

bool print_job(const char *text, uint32_t epoch) {
    static constexpr uint8_t init[] = {0x1B, '@'};
    if (!write_bytes(init, sizeof(init), epoch) || !print_logo(epoch)) return false;
    // Reassert text layout after raster printing without clearing printer buffers.
    static constexpr uint8_t text_mode[] = {
        0x1b, '!', 0, 0x1d, '!', 0, 0x1b, 'M', 0, 0x1b, '2', 0x1b, 'a', 0
    };
    if (!write_bytes(text_mode, sizeof(text_mode), epoch) || !feed_blank_line(epoch)) return false;
    detail::Formatter formatter(text);
    char line[kColumns + 1];
    size_t length = 0;
    while (!was_cancelled(epoch) && formatter.next(line, sizeof(line), length))
        if (!write_line(line, length, epoch)) return false;
    return !was_cancelled(epoch);
}

void printer_task(void *) {
    Job job{};
    for (;;) {
        if (xQueueReceive(queue_handle, &job, portMAX_DELAY) != pdTRUE) continue;
        const uint32_t epoch = job.cancel_epoch;
        set_busy(true);
        if (!is_ready()) {
            note_result(ESP_ERR_INVALID_STATE, false);
            outstanding_jobs.fetch_sub(1, std::memory_order_relaxed);
            continue;
        }
        const bool ok = print_job(job.text, epoch);
        const bool cancelled = was_cancelled(epoch);
        note_result(ok ? ESP_OK : (cancelled ? ESP_OK : ESP_FAIL), cancelled);
        outstanding_jobs.fetch_sub(1, std::memory_order_relaxed);
        if (!ok && !cancelled) ESP_LOGE(kTag, "UART print job failed");
    }
}
} // namespace

esp_err_t start(uint32_t baud) {
    if (started.load()) {
        const Status existing = status();
        return existing.ready ? ESP_OK : (existing.last_error == ESP_OK ? ESP_ERR_INVALID_STATE : existing.last_error);
    }
    if (baud < 1200 || baud > 115200) return ESP_ERR_INVALID_ARG;
    uart_config_t config{};
    config.baud_rate = static_cast<int>(baud);
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    // The sole printer worker already waits for physical drain after every
    // <=64-byte packet. Direct FIFO writes avoid asynchronous TX-ring data
    // still pending when wait_tx_done observes an idle hardware transmitter.
    esp_err_t err = uart_driver_install(kPort, 512, 0, 0, nullptr, 0);
    if (err != ESP_OK) return err;
    if ((err = uart_param_config(kPort, &config)) != ESP_OK ||
        (err = uart_set_pin(kPort, kTxPin, kRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) != ESP_OK) {
        uart_driver_delete(kPort);
        return err;
    }
    printer_baud = baud;
    queue_handle = xQueueCreate(kQueueDepth, sizeof(Job));
    if (!queue_handle) { uart_driver_delete(kPort); return ESP_ERR_NO_MEM; }
    if (xTaskCreate(printer_task, "death_printer", 3072, nullptr, 4, &worker_handle) != pdPASS) {
        vQueueDelete(queue_handle); queue_handle = nullptr; uart_driver_delete(kPort); return ESP_ERR_NO_MEM;
    }
    portENTER_CRITICAL(&state_lock);
    current.ready = true;
    current.last_error = ESP_OK;
    portEXIT_CRITICAL(&state_lock);
    started.store(true);
    ESP_LOGI(kTag, "Printer UART ready (TX=%d RX=%d baud=%lu)", kTxPin, kRxPin,
             static_cast<unsigned long>(baud));
    return ESP_OK;
}

bool enqueue(const char *fortune) {
    if (!started.load() || !fortune || !is_ready()) return false;
    const size_t length = strnlen(fortune, kMaxFortuneBytes + 1);
    if (length > kMaxFortuneBytes) return false;
    Job job{};
    job.cancel_epoch = cancel_epoch.load(std::memory_order_relaxed);
    std::memcpy(job.text, fortune, length);
    outstanding_jobs.fetch_add(1, std::memory_order_relaxed);
    if (xQueueSend(queue_handle, &job, 0) != pdTRUE) {
        outstanding_jobs.fetch_sub(1, std::memory_order_relaxed);
        return false;
    }
    const uint8_t waiting = static_cast<uint8_t>(uxQueueMessagesWaiting(queue_handle));
    portENTER_CRITICAL(&state_lock);
    current.queued = waiting;
    portEXIT_CRITICAL(&state_lock);
    return true;
}

void cancel() {
    if (!started.load()) return;
    cancel_epoch.fetch_add(1, std::memory_order_relaxed);
    Job discarded{};
    uint32_t removed = 0;
    while (xQueueReceive(queue_handle, &discarded, 0) == pdTRUE) ++removed;
    if (removed) outstanding_jobs.fetch_sub(removed, std::memory_order_relaxed);
    portENTER_CRITICAL(&state_lock);
    current.queued = 0;
    current.cancelled_jobs += removed;
    portEXIT_CRITICAL(&state_lock);
}

Status status() {
    const uint8_t waiting = queue_handle ? static_cast<uint8_t>(uxQueueMessagesWaiting(queue_handle)) : 0;
    portENTER_CRITICAL(&state_lock);
    Status result = current;
    result.queued = waiting;
    result.busy = result.busy || outstanding_jobs.load(std::memory_order_relaxed) != 0;
    portEXIT_CRITICAL(&state_lock);
    return result;
}

} // namespace skull::printer
