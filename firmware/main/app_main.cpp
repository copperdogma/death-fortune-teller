#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <esp_matter.h>
#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/PlatformManager.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include "skull_radio.h"
#include "skull_storage.h"
#include "skull_startup.h"
#include "skull_actuators.h"
#include "skull_audio.h"
#include "skull_catalog.h"
#include "death_show.h"
#include "death_fault_log.h"
#include "death_printer.h"
#include "death_fortune.h"


using namespace esp_matter;
using namespace chip::app::Clusters;
namespace {
const char *tag = "skull"; // Set once from validated SD config before starting tasks.
std::atomic<uint32_t> tone_frames{0};
std::atomic<uint32_t> callback_max_us{0};
std::atomic<uint32_t> triggers{0};
std::atomic<bool> matter_ble_ready{false};
uint16_t trigger_endpoint = 0;
uint16_t stop_endpoint = 0;
uint16_t waiting_endpoint = 0;
std::atomic<bool> mode_sync_requested{true}, mode_sync_queued{false};
std::atomic<bool> close_jaw_requested{false};
bool controls_armed = false; // Matter-thread only; boot hydration never triggers work.
bool internal_update = false; // Accessed only on Matter's system layer.
skull::storage::Config card_config;
skull::catalog::Catalog *assets = nullptr;
bool heap_window = false; // Diagnostic task only.

// Startup PCM has priority over the explicit diagnostic tone.
// No card I/O, logging, allocation or blocking in this provider.
int32_t provide_pcm(uint8_t *data, int32_t length) {
    if (!data || length <= 0) return 0;
    const int64_t started = esp_timer_get_time();
    std::memset(data, 0, static_cast<size_t>(length));
    const bool startup = skull::startup::provide(data, length);
    const bool show_audio = !startup && skull::audio::provide(data, length);
    const uint32_t remaining = tone_frames.load(std::memory_order_relaxed);
    const uint32_t count = startup || show_audio ? 0 : std::min(static_cast<uint32_t>(length / 4), remaining);
    static uint32_t phase = 0;
    for (uint32_t index = 0; index < count; ++index) {
        const int16_t sample = static_cast<int16_t>((phase < 50 ? static_cast<int>(phase) - 25 : 75 - static_cast<int>(phase)) * 20);
        std::memcpy(data + index * 4, &sample, 2);
        std::memcpy(data + index * 4 + 2, &sample, 2);
        phase = (phase + 1) % 100;
    }
    // Dead Stop may clear this counter concurrently; never subtract below zero.
    if (count) {
        uint32_t current = tone_frames.load(std::memory_order_relaxed);
        while (current && !tone_frames.compare_exchange_weak(current, current > count ? current - count : 0,
                                                              std::memory_order_relaxed)) {}
    }
    const uint32_t elapsed = static_cast<uint32_t>(esp_timer_get_time() - started);
    uint32_t maximum = callback_max_us.load(std::memory_order_relaxed);
    while (elapsed > maximum && !callback_max_us.compare_exchange_weak(maximum, elapsed, std::memory_order_relaxed)) {}
    return length;
}

bool request_tone() {
    if (!skull::radio::stream_ready() || skull::startup::busy() || skull::audio::busy() || skull::death::busy()) return false;
    uint32_t expected = 0;
    if (!tone_frames.compare_exchange_strong(expected, 13230)) return false;
    triggers.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void request_dead_stop() {
    close_jaw_requested.store(true);
    skull::startup::cancel();
    tone_frames.store(0, std::memory_order_relaxed);
    skull::death::cancel();
    mode_sync_requested.store(true);
}

void publish_mode(intptr_t) {
    mode_sync_queued.store(false);
    const bool running = skull::death::mode() == skull::death::Mode::start;
    internal_update = true;
    const uint16_t endpoints[] = {trigger_endpoint, stop_endpoint, waiting_endpoint};
    const bool values[] = {running, false, !running};
    for (unsigned i = 0; i < 3; ++i) {
        esp_matter_attr_val_t previous{};
        if (attribute::get_val(endpoints[i], OnOff::Id, OnOff::Attributes::OnOff::Id, &previous) == ESP_OK &&
            previous.type == ESP_MATTER_VAL_TYPE_BOOLEAN && previous.val.b == values[i]) continue;
        auto value = esp_matter_bool(values[i]);
        const esp_err_t result = attribute::update(endpoints[i], OnOff::Id, OnOff::Attributes::OnOff::Id, &value);
        if (result != ESP_OK && result != ESP_ERR_NOT_FINISHED) {
            mode_sync_requested.store(true);
            ESP_LOGW(tag, "Matter mode update endpoint=%u: %s", endpoints[i], esp_err_to_name(result));
        }
    }
    internal_update = false;
    ESP_LOGI(tag, "Matter state=%s Start=%d Waiting=%d DeadStop=0", running ? "Start" : "Waiting", running, !running);
}

void sync_mode_if_needed() {
    // Diagnostic-task owner; coalesced work executes after any incoming Matter write.
    static int last_mode = -1;
    const int mode = static_cast<int>(skull::death::mode());
    const bool forced = mode_sync_requested.exchange(false);
    if (mode == last_mode && !forced) return;
    if (mode_sync_queued.exchange(true)) { mode_sync_requested.store(true); return; }
    const CHIP_ERROR result = chip::DeviceLayer::PlatformMgr().ScheduleWork(publish_mode, 0);
    if (result == CHIP_NO_ERROR) last_mode = mode;
    else { mode_sync_queued.store(false); mode_sync_requested.store(true); }
}

esp_err_t attribute_update(attribute::callback_type_t type, uint16_t endpoint, uint32_t cluster,
                           uint32_t attribute_id, esp_matter_attr_val_t *value, void *) {
    if (type != attribute::PRE_UPDATE || internal_update || !controls_armed || !value ||
        (endpoint != trigger_endpoint && endpoint != stop_endpoint && endpoint != waiting_endpoint) ||
        cluster != OnOff::Id || attribute_id != OnOff::Attributes::OnOff::Id ||
        value->type != ESP_MATTER_VAL_TYPE_BOOLEAN) return ESP_OK;
    mode_sync_requested.store(true);
    if (!value->val.b) return ESP_OK; // Off is not an implicit Stop.
    if (endpoint == stop_endpoint) {
        request_dead_stop();
        ESP_LOGI(tag, "Matter Dead Stop accepted; returning to Waiting");
    } else if (endpoint == trigger_endpoint) {
        const bool accepted = skull::death::trigger();
        if (accepted) triggers.fetch_add(1, std::memory_order_relaxed);
        ESP_LOGI(tag, "Matter Start %s", accepted ? "accepted" : "ignored (startup/busy/cooldown/speaker unavailable)");
    } else ESP_LOGI(tag, "Matter Waiting is a state indicator; no trigger");
    return ESP_OK;
}

void label_control(endpoint_t *endpoint, const char *label, uint8_t code) {
    chip::app::DataModel::Provider::SemanticTag semantic{};
    semantic.mfgCode.SetNonNull(static_cast<chip::VendorId>(CONFIG_DEVICE_VENDOR_ID));
    semantic.namespaceID = 0;
    semantic.tag = code;
    semantic.label.SetValue(chip::app::DataModel::MakeNullable(chip::CharSpan::fromCharString(label)));
    ESP_ERROR_CHECK(cluster::descriptor::feature::tag_list::add(cluster::get(endpoint, Descriptor::Id)));
    ESP_ERROR_CHECK(endpoint::set_semantic_tags(endpoint, &semantic, 1));
}

esp_err_t identify(identification::callback_type_t, uint16_t, uint8_t, uint8_t, void *) { return ESP_OK; }

void matter_event(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t) {
    // GAP callback registration is earlier than advertising/address setup.
    // Don't start our scanner until uncommissioned Matter has finished it.
    if (event->Type == chip::DeviceLayer::DeviceEventType::kCHIPoBLEAdvertisingChange &&
        event->CHIPoBLEAdvertisingChange.Result == chip::DeviceLayer::kActivity_Started)
        matter_ble_ready.store(true);
    if (event->Type == chip::DeviceLayer::DeviceEventType::kCommissioningComplete)
        ESP_LOGI(tag, "Matter commissioning complete");
    else if (event->Type == chip::DeviceLayer::DeviceEventType::kBLEDeinitialized)
        ESP_LOGE(tag, "Unexpected Matter BLE deinitialization: retained Bluetooth gate FAILED");
    else if (event->Type == chip::DeviceLayer::DeviceEventType::kFabricRemoved)
        ESP_LOGI(tag, "Matter fabric removed; use explicit commissioning-window command to re-add");
}

void status() {
    skull::faultlog::status();
    const auto radio = skull::radio::stats();
    const auto audio = skull::audio::stats();
    const auto actuators = skull::actuators::stats();
    ESP_LOGI(tag, "ACTUATORS deferred=%lu max_pending_us=%llu pending_channels=%u stuck=%d",
             static_cast<unsigned long>(actuators.deferred_updates),
             static_cast<unsigned long long>(actuators.max_pending_us),
             static_cast<unsigned>(actuators.pending_channels), actuators.stuck);
    ESP_LOGI(tag, "PRODUCER blocks=%lu read_total_us=%lu read_max_us=%lu process_total_us=%lu process_max_us=%lu refill_gap_max_us=%lu role_frames=%lu raw_nonzero=%lu output_nonzero=%lu peak=%lu",
             static_cast<unsigned long>(audio.blocks), static_cast<unsigned long>(audio.read_us_total),
             static_cast<unsigned long>(audio.read_us_max), static_cast<unsigned long>(audio.process_us_total),
             static_cast<unsigned long>(audio.process_us_max), static_cast<unsigned long>(audio.refill_gap_us_max),
             static_cast<unsigned long>(audio.role_frames), static_cast<unsigned long>(audio.input_nonzero_samples),
             static_cast<unsigned long>(audio.output_nonzero_samples), static_cast<unsigned long>(audio.output_peak));
    ESP_LOGI(tag, "PCM raw_fnv1a=%08lx gain_percent=%u", static_cast<unsigned long>(audio.raw_pcm_fnv1a), skull::audio::gain_percent());
    const auto printer = skull::printer::status();
    ESP_LOGI(tag, "PRINTER UART_ready=%d busy=%d queued=%u sent=%lu failed=%lu cancelled=%lu error=%s (paper status unverified)",
             printer.ready, printer.busy, printer.queued, static_cast<unsigned long>(printer.completed_jobs),
             static_cast<unsigned long>(printer.failed_jobs), static_cast<unsigned long>(printer.cancelled_jobs),
             esp_err_to_name(printer.last_error));
    ESP_LOGI(tag, "SHOW state=%s PCM frames=%lu underruns=%lu producer_stack=%lu cancel_waiting=%d assets=%u heap_window=%d",
             skull::death::state_name(), static_cast<unsigned long>(audio.frames),
             static_cast<unsigned long>(audio.underruns), static_cast<unsigned long>(audio.stack_high_water),
             audio.cancel_waiting, assets ? static_cast<unsigned>(assets->count) : 0,
             heap_window);
    const auto interruption = skull::death::last_interruption();
    ESP_LOGI(tag, "LAST_INTERRUPTION count=%lu uptime_ms=%lld reason=%s phase=%s asset=%s (retained until reboot)",
             static_cast<unsigned long>(interruption.count), static_cast<long long>(interruption.uptime_us / 1000),
             interruption.reason, interruption.phase, interruption.asset);
    ESP_LOGI(tag, "STATUS sd=%d speaker=%d peer=%d tone_frames=%lu triggers=%lu callbacks=%lu bytes=%lu cb_max_us=%lu dropped=%lu internal=%u min_internal=%u largest=%u psram=%u stream=%d fault=%d",
             skull::storage::mounted(), skull::radio::speaker_connected(), skull::radio::peer_ready(),
             static_cast<unsigned long>(tone_frames.load()), static_cast<unsigned long>(triggers.load()),
             static_cast<unsigned long>(radio.audio_callbacks), static_cast<unsigned long>(radio.audio_bytes),
             static_cast<unsigned long>(callback_max_us.load()), static_cast<unsigned long>(radio.dropped_events),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), skull::radio::stream_ready(), radio.faulted);
    ESP_LOGI(tag, "STACK minimum unused bytes: radio=%lu diagnostic=%u; DMA free=%u",
             static_cast<unsigned long>(radio.stack_free_min),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));
}

void inspect_asset(const char *path) {
    if (!assets) return;
    for (size_t i = 0; i < assets->count; ++i) {
        const auto &asset = assets->assets[i];
        if (std::strcmp(path, asset.path)) continue;
        ESP_LOGI(tag, "ASSET role=%c path=%s frames=%lu lines=%u skit=%d",
                 card_config.primary ? 'A' : 'B', asset.path,
                 static_cast<unsigned long>(asset.wave.data_bytes / 4),
                 static_cast<unsigned>(asset.line_count), asset.skit);
        for (unsigned line = 0; line < asset.line_count; ++line) {
            const auto &interval = asset.lines[line];
            ESP_LOGI(tag, "ASSET interval=%u role=%c frames=%lu..%lu", line, interval.role,
                     static_cast<unsigned long>(interval.first_frame),
                     static_cast<unsigned long>(interval.end_frame));
        }
        return;
    }
    ESP_LOGW(tag, "ASSET not found: %s", path);
}

void pairing() {
    // Called only on Matter's system layer. Do not open an additional fabric
    // window on a commissioned device through this diagnostic command.
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0) {
        ESP_LOGI(tag, "Already commissioned; use Home's existing accessory");
        return;
    }
    auto &manager = chip::Server::GetInstance().GetCommissioningWindowManager();
    if (!manager.IsCommissioningWindowOpen()) {
        const CHIP_ERROR error = manager.OpenBasicCommissioningWindow(chip::System::Clock::Seconds32(300));
        if (error != CHIP_NO_ERROR) {
            ESP_LOGE(tag, "Commissioning window failed: %" CHIP_ERROR_FORMAT, error.Format());
            return;
        }
    }
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
}

void diagnostic_task(void *) {
    const int64_t deadline = esp_timer_get_time() + 10000000;
    esp_err_t attached = ESP_ERR_NOT_FOUND;
    while (attached == ESP_ERR_NOT_FOUND && esp_timer_get_time() < deadline) {
        attached = skull::radio::attach_matter_gap();
        if (attached == ESP_ERR_NOT_FOUND) vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (attached != ESP_OK) {
        ESP_LOGE(tag, "Matter GAP dispatcher readiness failed: %s; radio start withheld", esp_err_to_name(attached));
    } else {
        while (!matter_ble_ready.load() && esp_timer_get_time() < deadline)
            vTaskDelay(pdMS_TO_TICKS(50));
        if (!matter_ble_ready.load())
            ESP_LOGE(tag, "Matter advertising/address readiness timed out; radio startup gate FAILED");
    }
    if (attached == ESP_OK && matter_ble_ready.load() && *card_config.speaker_name) {
        skull::radio::Config config{card_config.speaker_name, "", true};
        ESP_ERROR_CHECK(skull::radio::start(config));
    }
    ESP_LOGI(tag, "Death ready. Commands: status, fault-log, fault-test, asset /audio/<file>.wav, heap-start, heap-stop, printer-health, print-test, tone, start, stop, waiting, cancel, jaw-test, retry, pairing, reboot, factory-reset CONFIRM. Matter controls: Start, Dead Stop, Waiting.");
    char line[256]{};
    size_t used = 0;
    bool overflow = false;
    int64_t next_status = 0;
    while (true) {
        skull::startup::poll(skull::radio::stream_ready());
        const bool tone_active = tone_frames.load(std::memory_order_relaxed) != 0;
        skull::death::tick(skull::startup::finished(), skull::startup::playing() || tone_active);
        sync_mode_if_needed();
        const auto startup_animation = skull::startup::animation();
        const auto show_animation = skull::audio::animation();
        const bool audio_active = skull::radio::stream_ready() &&
            (skull::startup::playing() || tone_active || skull::death::busy() || skull::audio::busy());
        skull::actuators::tick(startup_animation.rms ? startup_animation.rms : show_animation.rms,
                              startup_animation.speaking || show_animation.speaking, audio_active, close_jaw_requested.exchange(false) ? 0 : skull::death::jaw_override());
        skull::faultlog::poll(
            !skull::death::busy() && skull::audio::quiescent() && !skull::startup::busy() &&
            !skull::printer::busy() && tone_frames.load() == 0,
            skull::storage::mounted());
        char character;
        if (uart_read_bytes(UART_NUM_0, &character, 1, pdMS_TO_TICKS(20)) == 1) {
            if (character == '\r' || character == '\n') {
                if (overflow) { used = 0; overflow = false; continue; }
                line[used] = '\0';
                if (!std::strcmp(line, "status")) status();
                else if (!std::strcmp(line, "fault-log")) skull::faultlog::dump();
                else if (!std::strcmp(line, "fault-test") && !skull::death::busy() && skull::audio::quiescent() &&
                         !skull::startup::busy() && !skull::printer::busy() && tone_frames.load() == 0) skull::faultlog::test_fault();
                else if (!std::strncmp(line, "asset ", 6) && !skull::death::busy()) inspect_asset(line + 6);
                else if (!std::strcmp(line, "heap-start") && !heap_window) {
                    heap_window = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
                    ESP_LOGI(tag, "Local heap window %s", heap_window ? "started" : "failed");
                    status();
                }
                else if (!std::strcmp(line, "heap-stop") && heap_window) {
                    status(); // Record window minimum before restoring boot history.
                    ESP_LOGI(tag, "Local heap window stop: %s", esp_err_to_name(heap_caps_monitor_local_minimum_free_size_stop()));
                    heap_window = false;
                }
                else if (!std::strcmp(line, "printer-health") && !skull::death::busy() && !skull::printer::busy() && !skull::startup::busy()) {
                    // Idle diagnostic only, after a printer reset: DLE EOT3.
                    // Status is not an acknowledgement that its receive buffer is free.
                    uart_flush_input(UART_NUM_1);
                    const uint8_t query[] = {0x10, 0x04, 0x03};
                    uint8_t response = 0;
                    const int written = uart_write_bytes(UART_NUM_1, query, sizeof(query));
                    const esp_err_t drained = uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(100));
                    const int received = written == static_cast<int>(sizeof(query)) && drained == ESP_OK ?
                        uart_read_bytes(UART_NUM_1, &response, 1, pdMS_TO_TICKS(250)) : 0;
                    ESP_LOGI(tag, "PRINTER_HEALTH response_bytes=%d raw=%02x voltage_error=%d heat_or_voltage_error=%d (RX wiring/status not guaranteed)",
                             received, response, received == 1 && (response & 0x20), received == 1 && (response & 0x40));
                }
                else if (!std::strcmp(line, "print-test") && !skull::death::busy() && skull::audio::quiescent() && !skull::startup::busy() && !skull::printer::busy()) {
                    const auto fortune = skull::fortune::generate();
                    ESP_LOGI(tag, "Printer-only fortune %s", skull::printer::queue_fortune(fortune.c_str()) ? "queued" : "rejected");
                }
                else if (!std::strcmp(line, "tone")) ESP_LOGI(tag, "Serial tone %s", request_tone() ? "accepted" : "ignored");
                else if (!std::strcmp(line, "start") || !std::strcmp(line, "trigger")) ESP_LOGI(tag, "Serial Start %s", skull::death::trigger() ? "accepted" : "ignored");
                else if (!std::strcmp(line, "waiting")) ESP_LOGI(tag, "Waiting is a state indicator; current=%s", skull::death::state_name());
                else if (!std::strcmp(line, "stop") || !std::strcmp(line, "cancel")) { request_dead_stop(); ESP_LOGI(tag, "Dead Stop requested"); }
                else if (!std::strncmp(line, "play ", 5)) ESP_LOGI(tag, "Local diagnostic play %s", (tone_frames.load() == 0 && skull::death::local(line + 5)) ? "accepted" : "ignored");
                else if (!std::strcmp(line, "sd-raw") && !skull::death::busy() && skull::audio::quiescent() && !skull::startup::playing()) {
                    ESP_LOGI(tag, "SDRAW reference match=%d", skull::storage::probe_raw_reference());
                }
                else if ((!std::strcmp(line, "sd-window") || !std::strcmp(line, "sd-window-single")) && !skull::death::busy() && skull::audio::quiescent() && !skull::startup::playing()) {
                    ESP_LOGI(tag, "SDWINDOW reference match=%d", skull::startup::probe_window(!std::strcmp(line, "sd-window-single")));
                }
                else if ((!std::strcmp(line, "sd-check") || !std::strcmp(line, "sd-single")) && !skull::death::busy() && skull::audio::quiescent() && !skull::startup::playing()) {
                    for (unsigned pass = 1; pass <= 2; ++pass) {
                        const auto result = skull::startup::probe_file(!std::strcmp(line, "sd-single"));
                        ESP_LOGI(tag, "SDCHECK pass=%u ok=%d bytes=%lu fnv1a=%08lx nonzero=%lu peak=%lu", pass,
                                 result.ok, static_cast<unsigned long>(result.bytes), static_cast<unsigned long>(result.hash),
                                 static_cast<unsigned long>(result.nonzero_samples), static_cast<unsigned long>(result.peak));
                    }
                }
                else if (!std::strncmp(line, "sd-clock ", 9) && skull::storage::mounted() && !skull::death::busy() && skull::audio::quiescent() && !skull::startup::playing()) {
                    char *end = nullptr;
                    const unsigned long khz = std::strtoul(line + 9, &end, 10);
                    if (end && !*end && (khz == 400 || khz == 4000 || khz == 10000 || khz == 20000))
                        ESP_LOGI(tag, "SDCLOCK khz=%lu result=%s (diagnostic, resets on reboot)", khz,
                                 esp_err_to_name(sdmmc_host_set_card_clk(SDMMC_HOST_SLOT_1, khz)));
                }
                else if (!std::strncmp(line, "gain ", 5)) {
                    char *end = nullptr;
                    const unsigned long value = std::strtoul(line + 5, &end, 10);
                    const bool accepted = line[5] && end && !*end && value <= 100 &&
                        !skull::startup::playing() && !skull::death::busy() && skull::audio::set_gain_percent(static_cast<unsigned>(value));
                    ESP_LOGI(tag, "Diagnostic PCM gain %s; current=%u%%", accepted ? "accepted" : "ignored (idle and 0..100 required)", skull::audio::gain_percent());
                }
                else if (!std::strcmp(line, "jaw-test") && !skull::startup::busy() && !skull::death::busy() && !skull::audio::busy()) skull::actuators::test();
                else if (!std::strcmp(line, "retry") && tone_frames.load() == 0 && !(skull::startup::busy() && skull::radio::stream_ready()) && ((!skull::death::busy() && !skull::audio::busy()) ||
                         (skull::audio::stats().cancel_waiting && !skull::radio::stream_ready()))) skull::radio::retry_connections();
                else if (!std::strcmp(line, "pairing")) chip::DeviceLayer::SystemLayer().ScheduleLambda([]() { pairing(); });
                else if (!std::strcmp(line, "reboot")) {
                    // This task also schedules shows/prepare, so an accepted
                    // reboot cannot race a newly launched SD read. Waiting for
                    // initial speaker discovery is safe: startup PCM was loaded
                    // and its FILE closed before this task was created.
                    if (skull::death::busy() || !skull::audio::quiescent() ||
                        skull::startup::playing() || tone_frames.load() != 0 || skull::printer::busy())
                        ESP_LOGW(tag, "Reboot refused while show/audio/startup/tone or SD file active; retry when idle");
                    else esp_restart();
                }
                else if (!std::strcmp(line, "factory-reset CONFIRM") && !skull::death::busy() && skull::audio::quiescent() && !skull::printer::busy() && !skull::startup::busy()) {
                    chip::DeviceLayer::SystemLayer().ScheduleLambda([]() { esp_matter::factory_reset(); });
                }
                else if (used) ESP_LOGW(tag, "Unknown command");
                used = 0;
            } else if (!overflow && used < sizeof(line) - 1) line[used++] = character;
            else overflow = true;
        }
        if (esp_timer_get_time() >= next_status) {
            status();
            next_status = esp_timer_get_time() + 10000000;
        }
    }
}
}

extern "C" void app_main() {
    ESP_LOGI(tag, "Death single-WROVER firmware (onboard SD, A2DP, Matter, printer)");
    ESP_LOGI(tag, "Boot reset_reason=%d (esp_reset_reason_t)", static_cast<int>(esp_reset_reason()));
    ESP_ERROR_CHECK(skull::actuators::init_lights());
    ESP_ERROR_CHECK(nvs_flash_init()); // Never erase fabric/NVS automatically.
    skull::faultlog::init();
    ESP_LOGI(tag, "Boot reset_reason=%d (fault logger active)", static_cast<int>(esp_reset_reason()));
    ESP_LOGI(tag, "PSRAM initialized=%d size=%u", esp_psram_is_initialized(), static_cast<unsigned>(esp_psram_get_size()));
    // Configure PWM only after validated SD calibration is available. During
    // card errors leave servo outputs uninitialized rather than guess limits.
    esp_err_t sd = skull::storage::mount_and_read_config(card_config);
    while (sd != ESP_OK) {
        skull::faultlog::poll(true, false);
        const unsigned failure_blinks = skull::storage::mounted() ? 5 : 3;
        ESP_LOGW(tag, "Boot storage/config retry: %s; %u eye flashes", esp_err_to_name(sd), failure_blinks);
        skull::actuators::diagnostic(failure_blinks, true);
        const unsigned diagnostic_ticks = (skull::actuators::diagnostic_duration_ms(failure_blinks) + 19) / 20;
        for (unsigned i = 0; i < diagnostic_ticks; ++i) {
            skull::actuators::tick(0, false);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        // No audio file or producer exists yet; remount permits a repaired or
        // reinserted card to recover without stale host/card state.
        const esp_err_t unmount = skull::storage::unmount_for_retry();
        if (unmount != ESP_OK) ESP_LOGW(tag, "Retry unmount: %s", esp_err_to_name(unmount));
        sd = skull::storage::mount_and_read_config(card_config);
    }
    tag = "death";
    ESP_ERROR_CHECK(skull::actuators::configure(card_config.servo_us_min, card_config.servo_us_max, card_config.servo_reverse));
    ESP_ERROR_CHECK(skull::actuators::init());
    constexpr unsigned blinks = 4;
    skull::actuators::diagnostic(blinks);
    ESP_LOGI(tag, "Boot eye diagnostic: %u flashes", blinks);
    // Finish both the role flashes and gentle check before lengthy radio setup.
    const unsigned role_ticks = (skull::actuators::diagnostic_duration_ms(blinks, false) + 19) / 20;
    for (unsigned i = 0; i <= std::max(60u, role_ticks); ++i) {
        skull::actuators::tick(0, false);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (sd == ESP_OK) {
        skull::startup::load(true, card_config.volume_percent);
        assets = static_cast<skull::catalog::Catalog *>(heap_caps_malloc(sizeof(skull::catalog::Catalog), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (assets) new (assets) skull::catalog::Catalog;
        if (!assets || !skull::catalog::scan(*assets, "/sdcard/audio")) {
            ESP_LOGE(tag, "Audio catalog unavailable");
            if (assets) heap_caps_free(assets);
            assets = nullptr;
        }
        if (assets && !skull::audio::init(card_config.volume_percent, true)) {
            ESP_LOGE(tag, "PCM producer initialization failed; skit control disabled");
            heap_caps_free(assets); assets = nullptr;
        }
    }
    skull::death::Config death_config;
    death_config.cooldown_ms = card_config.cooldown_ms;
    static char fortune_path[160];
    std::snprintf(fortune_path, sizeof fortune_path, "/sdcard%s", card_config.fortunes_json);
    death_config.fortunes_path = fortune_path;
    skull::death::init(assets, death_config);
    const esp_err_t printer = skull::printer::start(card_config.printer_baud);
    if (printer != ESP_OK) ESP_LOGE(tag, "Printer UART unavailable: %s", esp_err_to_name(printer));
    ESP_ERROR_CHECK(skull::radio::init(provide_pcm));
    {
        node::config_t node_config{};
        std::strcpy(node_config.root_node.basic_information.node_label, "Death fortune teller");
        node_t *root = node::create(&node_config, attribute_update, identify);
        ESP_ERROR_CHECK(root ? ESP_OK : ESP_FAIL);
        endpoint::on_off_plug_in_unit::config_t trigger_config{};
        trigger_config.on_off.on_off = false;
        endpoint_t *trigger = endpoint::on_off_plug_in_unit::create(root, &trigger_config, ENDPOINT_FLAG_NONE, nullptr);
        ESP_ERROR_CHECK(trigger ? ESP_OK : ESP_FAIL);
        trigger_endpoint = endpoint::get_id(trigger);
        endpoint_t *stop = endpoint::on_off_plug_in_unit::create(root, &trigger_config, ENDPOINT_FLAG_NONE, nullptr);
        ESP_ERROR_CHECK(stop ? ESP_OK : ESP_FAIL);
        stop_endpoint = endpoint::get_id(stop);
        trigger_config.on_off.on_off = true;
        endpoint_t *waiting = endpoint::on_off_plug_in_unit::create(root, &trigger_config, ENDPOINT_FLAG_NONE, nullptr);
        ESP_ERROR_CHECK(waiting ? ESP_OK : ESP_FAIL);
        waiting_endpoint = endpoint::get_id(waiting);
        label_control(trigger, "Start", 1);
        label_control(stop, "Dead Stop", 2);
        label_control(waiting, "Waiting", 3);
        ESP_LOGI(tag, "Matter endpoints: Start=%u Dead Stop=%u Waiting=%u", trigger_endpoint, stop_endpoint, waiting_endpoint);
        ESP_ERROR_CHECK(esp_matter::start(matter_event));
        chip::DeviceLayer::SystemLayer().ScheduleLambda([]() {
            publish_mode(0); // Hydrated On never starts a show.
            controls_armed = true;
            // Commissioned boot does not advertise or set a random address until
            // a future explicit commissioning window. Death starts no skull peer discovery.
            if (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0)
                matter_ble_ready.store(true);
            else pairing();
        });
    }
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(xTaskCreate(diagnostic_task, "skull_diag", 6144, nullptr, 4, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
