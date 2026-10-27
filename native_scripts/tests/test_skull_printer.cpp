#include "death_printer.h"
#include "death_printer_logo.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "driver/uart.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace fake_uart {
std::mutex mutex;
std::condition_variable changed;
std::string transmitted;
bool block_write = false, entered_write = false;
bool block_drain = false, entered_drain = false, fail_drain = false;
unsigned writes = 0, drains = 0, block_write_at = 0;
bool last_write_logo = false, last_write_feed = false;
uint32_t configured_baud = 0;
TickType_t max_drain_timeout = 0;
}

esp_err_t uart_driver_install(uart_port_t, int rx_bytes, int tx_bytes, int, void *, int) {
    assert(rx_bytes == 512 && tx_bytes == 0); // Sole worker uses synchronous FIFO + explicit drain.
    return ESP_OK;
}
esp_err_t uart_param_config(uart_port_t, const uart_config_t *cfg) {
    fake_uart::configured_baud = static_cast<uint32_t>(cfg->baud_rate); return ESP_OK;
}
esp_err_t uart_set_pin(uart_port_t, int, int, int, int) { return ESP_OK; }
esp_err_t uart_driver_delete(uart_port_t) { return ESP_OK; }
int uart_write_bytes(uart_port_t, const void *bytes, size_t length) {
    std::unique_lock<std::mutex> lock(fake_uart::mutex);
    ++fake_uart::writes; fake_uart::entered_write = true; fake_uart::changed.notify_all();
    fake_uart::changed.wait(lock, [] { return !fake_uart::block_write && fake_uart::writes != fake_uart::block_write_at; });
    const auto *data = static_cast<const uint8_t *>(bytes);
    fake_uart::last_write_logo = length == skull::printer::detail::logo::packet_bytes && data[0] == 0x1d && data[1] == 0x76;
    fake_uart::last_write_feed = length == 3 && data[0] == 0x1b && data[1] == 'J';
    fake_uart::transmitted.append(static_cast<const char *>(bytes), length);
    return static_cast<int>(length);
}
esp_err_t uart_wait_tx_done(uart_port_t, TickType_t timeout) {
    std::unique_lock<std::mutex> lock(fake_uart::mutex);
    ++fake_uart::drains;
    fake_uart::entered_drain = true; fake_uart::changed.notify_all();
    fake_uart::changed.wait(lock, [] { return !fake_uart::block_drain; });
    if (timeout > fake_uart::max_drain_timeout) fake_uart::max_drain_timeout = timeout;
    return fake_uart::fail_drain ? ESP_FAIL : ESP_OK;
}

static bool wait_until(const std::function<bool()> &predicate, int timeout_ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return predicate();
}

static std::vector<std::string> formatted(const char *text) {
    skull::printer::detail::Formatter formatter(text);
    std::vector<std::string> lines;
    char line[33]; size_t length = 0;
    while (formatter.next(line, sizeof(line), length)) lines.emplace_back(line, length);
    return lines;
}

static void formatter_cases() {
    assert((formatted("hello world\nnext") == std::vector<std::string>{
        "Your fortune:", "", "hello world", "next", "", "-- Death, the Fortune-Teller", "", "", "", "", ""}));
    assert((formatted("first\n\nthird\n") == std::vector<std::string>{
        "Your fortune:", "", "first", "", "third", "", "", "-- Death, the Fortune-Teller", "", "", "", "", ""}));
    assert((formatted("hello world this is a deliberately long fortune") == std::vector<std::string>{
        "Your fortune:", "", "hello world this is a", "deliberately long fortune", "", "-- Death, the Fortune-Teller", "", "", "", "", ""}));
    const std::string long_word(35, 'x');
    assert((formatted(long_word.c_str()) == std::vector<std::string>{
        "Your fortune:", "", std::string(32, 'x'), "xxx", "", "-- Death, the Fortune-Teller", "", "", "", "", ""}));
    const char controls[] = {'A', '\x1b', '[', '3', '1', 'm', '\x7f', ' ', char(0xc3), char(0xa9), 0};
    assert((formatted(controls) == std::vector<std::string>{
        "Your fortune:", "", "A?[31m? ??", "", "-- Death, the Fortune-Teller", "", "", "", "", ""}));
}

static void logo_cases() {
    using namespace skull::printer::detail::logo;
    assert(width_bytes == 24 && height == 177 && packet_bytes == 32);
    uint8_t packet[packet_bytes];
    uint32_t hash = 2166136261u;
    for (unsigned row = 0; row < height; ++row) {
        assert(row_packet(row, packet));
        const uint8_t expected[] = {0x1d,0x76,0x30,0x00,24,0,1,0};
        assert(std::memcmp(packet, expected, sizeof(expected)) == 0);
        assert(std::memcmp(packet + 8, pixels + row * width_bytes, width_bytes) == 0);
        for (unsigned i = 8; i < packet_bytes; ++i) hash = (hash ^ packet[i]) * 16777619u;
    }
    assert(!row_packet(height, packet));
    // Fixed checksum of half-size top-down logo pixels, including all zero bytes.
    assert(hash == 0xa45dd837u);
}

static void worker_cases() {
    using namespace skull::printer;
    std::mutex gap_mutex;
    std::condition_variable gap_changed;
    unsigned logo_gaps = 0, hold_gap_at = 0, feed_gaps = 0, hold_feed_gap_at = 0;
    bool entered_gap = false, entered_feed_gap = false;
    printer_stub::set_delay_hook([&](TickType_t ticks) {
        if (ticks != pdMS_TO_TICKS(50)) return;
        bool logo, feed;
        {
            std::lock_guard<std::mutex> lock(fake_uart::mutex);
            assert(fake_uart::drains == fake_uart::writes);
            logo = fake_uart::last_write_logo; feed = fake_uart::last_write_feed;
        }
        std::unique_lock<std::mutex> lock(gap_mutex);
        if (logo) {
            ++logo_gaps;
            if (hold_gap_at == logo_gaps) {
                entered_gap = true; gap_changed.notify_all();
                gap_changed.wait(lock, [&] { return hold_gap_at != logo_gaps; });
            }
        } else if (feed) {
            ++feed_gaps;
            if (hold_feed_gap_at == feed_gaps) {
                entered_feed_gap = true; gap_changed.notify_all();
                gap_changed.wait(lock, [&] { return hold_feed_gap_at != feed_gaps; });
            }
        } else assert(false); // Every50ms gap belongs to a drained row or feed.
    });
    assert(start(0) == ESP_ERR_INVALID_ARG);
    assert(start(19200) == ESP_OK && fake_uart::configured_baud == 19200);
    assert(status().ready);
    const std::string overlong(kMaxFortuneBytes + 1, 'z');
    assert(!enqueue(overlong.c_str()));

    std::mutex handoff_mutex;
    std::condition_variable handoff_changed;
    bool dequeued = false, resume_worker = false;
    printer_stub::set_receive_hook([&] {
        std::unique_lock<std::mutex> lock(handoff_mutex);
        dequeued = true; handoff_changed.notify_all();
        handoff_changed.wait(lock, [&] { return resume_worker; });
    });
    assert(enqueue("hello world\nnext"));
    {
        std::unique_lock<std::mutex> lock(handoff_mutex);
        assert(handoff_changed.wait_for(lock, std::chrono::seconds(2), [&] { return dequeued; }));
    }
    assert(status().queued == 0);
    assert(status().busy && busy()); // Dequeued, but worker has not set its active flag yet.
    {
        std::lock_guard<std::mutex> lock(handoff_mutex);
        resume_worker = true; handoff_changed.notify_all();
    }
    printer_stub::set_receive_hook({});
    assert(wait_until([] { return status().completed_jobs == 1; }));
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        const std::string mode("\x1b!\0\x1d!\0\x1bM\0\x1b" "2\x1b" "a\0",14);
        const std::string feed("\x1bJ\x1e",3);
        const std::string expected = mode + feed + "Your fortune:\n" + feed +
            "hello world\nnext\n" + feed + "-- Death, the Fortune-Teller\n" + feed+feed+feed+feed+feed;
        using namespace skull::printer::detail::logo;
        assert(fake_uart::transmitted.size() > 5 + height * packet_bytes);
        assert(fake_uart::transmitted.substr(0,5) == std::string("\x1b@\x1b" "a\x01",5));
        uint8_t packet[packet_bytes];
        for (unsigned row = 0; row < height; ++row) {
            assert(row_packet(row, packet));
            assert(std::memcmp(fake_uart::transmitted.data() + 5 + row * packet_bytes, packet, packet_bytes) == 0);
        }
        assert(fake_uart::transmitted.substr(5 + height * packet_bytes) == expected);
        assert(fake_uart::drains == fake_uart::writes);
        assert(fake_uart::max_drain_timeout >= 100);
    }

    { std::lock_guard<std::mutex> lock(gap_mutex); assert(logo_gaps == detail::logo::height); assert(feed_gaps == 16); }

    // Cancel during a raster row: the current32-byte packet completes, but
    // no partial GS v0 payload or fortune text remains for the following job.
    size_t logo_start;
    unsigned held_write;
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        logo_start = fake_uart::transmitted.size();
        held_write = fake_uart::writes + 5; // ESC@, then four complete row packets.
        fake_uart::block_write_at = held_write;
    }
    assert(enqueue("must not print cancelled text"));
    assert(wait_until([&] { std::lock_guard<std::mutex> lock(fake_uart::mutex); return fake_uart::writes == held_write; }));
    cancel();
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        fake_uart::block_write_at = 0; fake_uart::changed.notify_all();
    }
    assert(wait_until([] { return status().cancelled_jobs == 1 && !status().busy; }));
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        using namespace skull::printer::detail::logo;
        assert(fake_uart::transmitted.size() - logo_start == 5 + 4 * packet_bytes);
        uint8_t packet[packet_bytes];
        for (unsigned row = 0; row < 4; ++row) {
            assert(row_packet(row, packet));
            assert(std::memcmp(fake_uart::transmitted.data() + logo_start + 5 + row * packet_bytes, packet, packet_bytes) == 0);
        }
        assert(fake_uart::transmitted.find("must not print cancelled text", logo_start) == std::string::npos);
    }
    assert(enqueue("after raster cancellation"));
    assert(wait_until([] { return status().completed_jobs == 2 && !status().busy; }));

    // Cancel while the receiver-processing gap is held. No following row or
    // fortune bytes may be submitted after cancellation releases the gap.
    {
        std::lock_guard<std::mutex> lock(gap_mutex);
        hold_gap_at = logo_gaps + 3; entered_gap = false;
    }
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        logo_start = fake_uart::transmitted.size();
    }
    assert(enqueue("must not print after gap cancellation"));
    {
        std::unique_lock<std::mutex> lock(gap_mutex);
        assert(gap_changed.wait_for(lock, std::chrono::seconds(2), [&] { return entered_gap; }));
    }
    unsigned writes_at_gap;
    { std::lock_guard<std::mutex> lock(fake_uart::mutex); writes_at_gap = fake_uart::writes; }
    cancel();
    { std::lock_guard<std::mutex> lock(gap_mutex); hold_gap_at = 0; gap_changed.notify_all(); }
    assert(wait_until([] { return status().cancelled_jobs == 2 && !status().busy; }));
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        assert(fake_uart::writes == writes_at_gap);
        assert(fake_uart::transmitted.size() - logo_start == 5 + 3 * detail::logo::packet_bytes);
        assert(fake_uart::transmitted.find("must not print after gap cancellation", logo_start) == std::string::npos);
    }

    // Stop during the first50ms quantum of a physical feed. The next quantum
    // and all following text/feeds must be suppressed, with command framing intact.
    {
        std::lock_guard<std::mutex> lock(gap_mutex);
        hold_feed_gap_at = feed_gaps + 1; entered_feed_gap = false;
    }
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        logo_start = fake_uart::transmitted.size();
    }
    assert(enqueue("must not print after feed cancellation"));
    {
        std::unique_lock<std::mutex> lock(gap_mutex);
        assert(gap_changed.wait_for(lock, std::chrono::seconds(2), [&] { return entered_feed_gap; }));
    }
    { std::lock_guard<std::mutex> lock(fake_uart::mutex); writes_at_gap = fake_uart::writes; }
    unsigned gaps_at_cancel;
    { std::lock_guard<std::mutex> lock(gap_mutex); gaps_at_cancel = feed_gaps; }
    cancel();
    { std::lock_guard<std::mutex> lock(gap_mutex); hold_feed_gap_at = 0; gap_changed.notify_all(); }
    assert(wait_until([] { return status().cancelled_jobs == 3 && !status().busy; }));
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        assert(fake_uart::writes == writes_at_gap);
        assert(fake_uart::transmitted.find("Your fortune:", logo_start) == std::string::npos);
        assert(fake_uart::transmitted.size()-logo_start == 5+detail::logo::height*detail::logo::packet_bytes+14+3);
    }
    { std::lock_guard<std::mutex> lock(gap_mutex); assert(feed_gaps == gaps_at_cancel); }

    // Hold one transport call so the active job and both queued slots are deterministic.
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        fake_uart::block_write = true; fake_uart::entered_write = false;
    }
    assert(enqueue("cancel active"));
    assert(wait_until([] { std::lock_guard<std::mutex> lock(fake_uart::mutex); return fake_uart::entered_write; }));
    assert(enqueue("queued one"));
    assert(enqueue("queued two"));
    assert(!enqueue("queue overflow"));
    assert(status().queued == 2);
    cancel();
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        fake_uart::block_write = false; fake_uart::changed.notify_all();
    }
    assert(wait_until([] { return status().cancelled_jobs >= 6 && !status().busy; }));
    assert(status().queued == 0);

    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        fake_uart::block_drain = true;
        fake_uart::entered_drain = false;
        fake_uart::fail_drain = false;
    }
    assert(enqueue("bounded drain failure"));
    assert(wait_until([] { std::lock_guard<std::mutex> lock(fake_uart::mutex); return fake_uart::entered_drain; }));
    unsigned writes_at_failed_drain;
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        writes_at_failed_drain = fake_uart::writes;
    }
    assert(enqueue("must not write after drain failure"));
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        fake_uart::fail_drain = true;
        fake_uart::block_drain = false;
        fake_uart::changed.notify_all();
    }
    assert(wait_until([] { return status().failed_jobs == 2 && !status().busy; }));
    assert(!status().ready);
    assert(status().last_error == ESP_FAIL);
    assert(start(19200) == ESP_FAIL);
    assert(!enqueue("must remain rejected"));
    {
        std::lock_guard<std::mutex> lock(fake_uart::mutex);
        assert(fake_uart::writes == writes_at_failed_drain);
    }
}

int main() {
    formatter_cases();
    logo_cases();
    worker_cases();
}
