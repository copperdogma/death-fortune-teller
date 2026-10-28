#include "skull_radio.h"
#include "skull_secondary.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gap_bt_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

namespace skull::radio {
namespace {
constexpr char TAG[] = "skull_radio";
constexpr uint16_t APP_ID = 0x534b;
constexpr size_t MAX_FILE = 240;
constexpr char SECONDARY_NAME[] = "SkullSecondary-Server";
// Bluedroid's UUID128 byte order is little endian.
const esp_bt_uuid_t SERVICE = {ESP_UUID_LEN_128, {.uuid128 = {
    0x4b,0x91,0x31,0xc3,0xc9,0xc5,0xcc,0x8f,0x9e,0x45,0xb5,0x1f,0x01,0xc2,0xaf,0x4f}}};
const esp_bt_uuid_t CHARACTERISTIC = {ESP_UUID_LEN_128, {.uuid128 = {
    0xa8,0x26,0x1b,0x36,0x07,0xea,0xf5,0xb7,0x88,0x46,0xe1,0x36,0x3e,0x48,0xb5,0xbe}}};
const esp_bt_uuid_t CCCD = {ESP_UUID_LEN_16, {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}};
enum class Kind { start, retry, peer_discovery, request, ble_gap, gatt, classic, a2dp };
// All pointer-bearing callback data is flattened before the stack callback returns.
struct Event {
    Kind kind{};
    int code{}, status{}, a{}, b{}, c{}, iface{};
    esp_a2d_cie_sbc_t sbc{};
    esp_bd_addr_t address{};
    char text[249]{};
};
QueueHandle_t queue;
StaticQueue_t queue_control; // Internal: scheduler timeout/event-list bookkeeping.
TaskHandle_t worker_task;
PcmCallback pcm;
esp_gap_ble_cb_t matter_gap;
std::atomic<bool> initialized{false}, started{false}, attached{false};
std::atomic<bool> standalone_gap{false};
std::atomic<bool> speaker_up{false}, peer_up{false}, overflow{false};
std::atomic<bool> media_ready{false}, faulted{false}, scan_active{false};
std::atomic<int> own_iface{ESP_GATT_IF_NONE};
std::atomic<PeerResult> result{PeerResult::idle};
std::atomic<uint32_t> callbacks{0}, bytes{0}, drops{0}, requests{0}, accepted{0};
// Everything below is exclusively owned by the worker after start is enqueued.
char speaker_name[249]{};
esp_bd_addr_t selected_peer{}, speaker_address{}, peer_address{};
bool primary_role = true;
bool fixed_peer{}, scan_configured{}, scanning{}, scan_setting{}, opening{}, have_peer{}, inquiry{}, found_speaker{}, speaker_connecting{}, streaming{};
bool closing{};
bool peer_discovery_enabled{}, scan_starting{}, scan_stopping{};
int64_t scan_retry_at{};
unsigned scan_failures{};
bool known_speaker{}, saved_speaker{};
unsigned reconnect_attempts{};
int64_t reconnect_at{};
bool speaker_acl_up{}, await_acl_down{};
uint16_t speaker_acl_handle{};
int64_t acl_wait_deadline{};
int64_t speaker_connect_deadline{};
int64_t close_deadline{}, media_retry_at{};
unsigned media_attempts{};
uint16_t connection{}, service_start{}, service_end{}, characteristic{}, descriptor{}, mtu{23};
uint16_t prepared{}, preparing{};
bool long_write{}, notification_seen{}, read_issued{}, initial_read{}, abandon_open{};
char pending[MAX_FILE + 1]{};
int64_t deadline{}, read_at{}, setup_deadline{};

bool post(const Event &event) {
    if (queue && xQueueSend(queue, &event, 0) == pdTRUE) return true;
    ++drops;
    overflow.store(true);
    return false;
}
void failed(const char *what, esp_err_t error) {
    if (error != ESP_OK) ESP_LOGW(TAG, "%s: %s", what, esp_err_to_name(error));
}
esp_gatt_if_t iface() { return static_cast<esp_gatt_if_t>(own_iface.load()); }
bool same(const uint8_t *a, const uint8_t *b) { return std::memcmp(a, b, ESP_BD_ADDR_LEN) == 0; }
int64_t now() { return esp_timer_get_time(); }
// One atomic NVS blob binds the address to the SD-selected name. Never inherit
// an unrelated speaker simply because this physical board used to be Jeff.
struct SpeakerCache { char name[249]; uint8_t address[6]; };
void load_speaker() {
    nvs_handle_t handle;
    if (nvs_open("skull_radio", NVS_READONLY, &handle) != ESP_OK) return;
    SpeakerCache cache{};
    size_t length = sizeof(cache);
    const esp_err_t error = nvs_get_blob(handle, "speaker", &cache, &length);
    nvs_close(handle);
    const esp_bd_addr_t empty{};
    if (error == ESP_OK && length == sizeof(cache) &&
        std::memchr(cache.name, 0, sizeof(cache.name)) &&
        !std::strcmp(cache.name, speaker_name) && !same(cache.address, empty)) {
        std::memcpy(speaker_address, cache.address, 6);
        known_speaker = saved_speaker = true;
        ESP_LOGI(TAG, "Loaded previously connected configured speaker");
    }
}
void save_speaker() {
    if (saved_speaker) return;
    SpeakerCache cache{};
    std::strcpy(cache.name, speaker_name);
    std::memcpy(cache.address, speaker_address, 6);
    nvs_handle_t handle;
    esp_err_t error = nvs_open("skull_radio", NVS_READWRITE, &handle);
    if (error == ESP_OK) {
        error = nvs_set_blob(handle, "speaker", &cache, sizeof(cache));
        if (error == ESP_OK) error = nvs_commit(handle);
        nvs_close(handle);
    }
    saved_speaker = error == ESP_OK;
    failed("remember configured speaker", error);
    if (saved_speaker) ESP_LOGI(TAG, "Remembered configured speaker for reboot reconnect");
}
void close_peer();
void finish(PeerResult value) {
    if (result.load() != PeerResult::pending) return;
    if (value == PeerResult::timed_out || value == PeerResult::rejected) peer_up.store(false);
    result.store(value);
    deadline = read_at = 0;
    long_write = notification_seen = read_issued = false;
    if (value == PeerResult::accepted) ++accepted;
    ESP_LOGI(TAG, "Jeff request result=%d (acceptance is not playback proof)", static_cast<int>(value));
    // The legacy protocol has no transaction ID. After an uncertain/error
    // result, discard this connection before accepting a subsequent request.
    if (value == PeerResult::timed_out || value == PeerResult::rejected) close_peer();
}
void close_peer() {
    peer_up.store(false);
    initial_read = false;
    if (result.load() == PeerResult::pending) finish(PeerResult::disconnected);
    closing = have_peer || opening;
    close_deadline = closing ? now() + 5000000 : 0;
    if (have_peer) {
        const esp_err_t error = esp_ble_gattc_close(iface(), connection);
        failed("close Jeff", error);
        if (error != ESP_OK) faulted.store(true);
    }
    else if (opening) abandon_open = true; // IDF has no cancel-open API; close its eventual result.
    setup_deadline = 0;
}
// Scanner state is worker-owned; the public snapshot becomes true only after
// the controller confirms scanning, and stays true until it confirms stopping.
void scan_backoff(bool failed_attempt) {
    if (failed_attempt) scan_failures = std::min(scan_failures + 1, 4u);
    else scan_failures = 0;
    scan_retry_at = now() + (2000000LL << scan_failures); // 2..32 seconds.
}
void stop_scan() {
    if (!scanning || scan_stopping) return;
    const esp_err_t error = esp_ble_gap_stop_scanning();
    failed("stop BLE scan", error);
    scan_stopping = error == ESP_OK;
    if (error != ESP_OK) scan_backoff(true);
}
void seek_peer() {
    if (!primary_role || !peer_discovery_enabled || faulted.load() || !attached.load() ||
        iface() == ESP_GATT_IF_NONE || have_peer || opening || closing ||
        scanning || scan_starting || scan_stopping || scan_setting ||
        (scan_retry_at && now() < scan_retry_at)) return;
    scan_retry_at = 0;
    if (!scan_configured) {
        esp_ble_scan_params_t p{};
        p.scan_type = BLE_SCAN_TYPE_ACTIVE;
        p.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
        p.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
        p.scan_interval = 0x50;
        p.scan_window = 0x30;
        p.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
        scan_setting = true;
        const esp_err_t error = esp_ble_gap_set_scan_params(&p);
        if (error != ESP_OK) { scan_setting = false; scan_backoff(true); failed("BLE scan params", error); }
    } else {
        const esp_err_t error = esp_ble_gap_start_scanning(10);
        scan_starting = error == ESP_OK;
        if (error != ESP_OK) scan_backoff(true);
        failed("BLE scan", error);
    }
}
// Retry forever, but serialize attempts using stack-confirmed connection/ACL
// state. Each episode tries the cached address three times, then name discovery.
void schedule_speaker_retry(bool fresh_episode = false) {
    if (fresh_episode) reconnect_attempts = 0;
    reconnect_at = now() + 5000000;
}
void seek() {
    if (faulted.load() || !attached.load() || (primary_role && iface() == ESP_GATT_IF_NONE)) return;
    // BLE discovery is application-gated; Classic speaker discovery/reconnect
    // remains independent so the initialization WAV can play first.
    seek_peer();
    if (speaker_name[0] && !speaker_up.load() && !speaker_connecting && !inquiry) {
        if (await_acl_down || speaker_acl_up || (reconnect_at && now() < reconnect_at)) return;
        if (known_speaker && reconnect_attempts < 3) {
            ++reconnect_attempts;
            reconnect_at = 0;
            const esp_err_t error = esp_a2d_source_connect(speaker_address);
            speaker_connecting = error == ESP_OK;
            if (speaker_connecting) speaker_connect_deadline = now() + 75000000;
            failed("configured speaker reconnect", error);
            if (speaker_connecting) {
                ESP_LOGI(TAG, "Remembered configured speaker reconnect attempt %u/3", reconnect_attempts);
                return;
            }
            if (reconnect_attempts < 3) { reconnect_at = now() + 5000000; return; }
        }
        found_speaker = false;
        esp_err_t e = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 8, 0);
        inquiry = e == ESP_OK;
        failed("speaker inquiry", e);
        if (e == ESP_OK) ESP_LOGI(TAG, "Speaker inquiry requested for '%s' (10.24 seconds)", speaker_name);
        else schedule_speaker_retry(true);
    }
}
void app_ble_gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *p) {
    Event e{}; e.kind = Kind::ble_gap; e.code = event;
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: e.status = p->scan_param_cmpl.status; break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: e.status = p->scan_start_cmpl.status; break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: e.status = p->scan_stop_cmpl.status; break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        e.a = p->scan_rst.search_evt;
        if (p->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            std::memcpy(e.address, p->scan_rst.bda, sizeof(e.address));
            e.b = p->scan_rst.ble_addr_type;
            uint8_t length = 0;
            auto *name = esp_ble_resolve_adv_data(p->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &length);
            if (!name) name = esp_ble_resolve_adv_data(p->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &length);
            if (!name && p->scan_rst.scan_rsp_len) {
                name = esp_ble_resolve_adv_data(p->scan_rst.ble_adv + p->scan_rst.adv_data_len, ESP_BLE_AD_TYPE_NAME_CMPL, &length);
            }
            if (name) std::memcpy(e.text, name, std::min<size_t>(length, sizeof(e.text) - 1));
        }
        break;
    }
    default: return;
    }
    post(e);
}
void gap_dispatch(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *p) {
    // Matter owns GATTS and its GAP events. Never replace or recursively wrap it.
    auto forward = matter_gap;
    // With the Matter commissioner disabled these four scanner events belong
    // solely to our client. The pinned Matter handler otherwise logs every
    // advertisement and schedules DriveBLEState for it, starving useful work.
    // Do not filter advertising/scan-response-configuration or security events.
#ifdef CONFIG_ENABLE_ESP32_BLE_CONTROLLER
#error "Matter commissioner requires a revised shared scanner dispatcher"
#endif
    const bool scanner_event = event == ESP_GAP_BLE_SCAN_RESULT_EVT ||
        event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT ||
        event == ESP_GAP_BLE_SCAN_START_COMPLETE_EVT || event == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT;
    if (!scanner_event && forward && forward != gap_dispatch) forward(event, p);
    if (standalone_gap.load()) secondary::gap_event(event, p);
    app_ble_gap(event, p);
}
void gatt_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gi, esp_ble_gattc_cb_param_t *p) {
    if (event == ESP_GATTC_REG_EVT) {
        if (p->reg.app_id != APP_ID) return;
    } else if (gi != own_iface.load() || gi == ESP_GATT_IF_NONE) return;
    Event e{}; e.kind = Kind::gatt; e.code = event; e.iface = gi;
    switch (event) {
    case ESP_GATTC_REG_EVT: e.status = p->reg.status; break;
    case ESP_GATTC_OPEN_EVT:
        e.status = p->open.status; e.a = p->open.conn_id; e.b = p->open.mtu;
        std::memcpy(e.address, p->open.remote_bda, sizeof(e.address)); break;
    case ESP_GATTC_CFG_MTU_EVT: e.status = p->cfg_mtu.status; e.a = p->cfg_mtu.conn_id; e.b = p->cfg_mtu.mtu; break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT: e.status = p->dis_srvc_cmpl.status; e.a = p->dis_srvc_cmpl.conn_id; break;
    case ESP_GATTC_SEARCH_RES_EVT:
        e.a = p->search_res.conn_id; e.b = p->search_res.start_handle; e.c = p->search_res.end_handle;
        if (p->search_res.srvc_id.uuid.len != ESP_UUID_LEN_128 || std::memcmp(p->search_res.srvc_id.uuid.uuid.uuid128, SERVICE.uuid.uuid128, 16)) return;
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT: e.status = p->search_cmpl.status; e.a = p->search_cmpl.conn_id; break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: e.status = p->reg_for_notify.status; e.b = p->reg_for_notify.handle; break;
    case ESP_GATTC_WRITE_DESCR_EVT: case ESP_GATTC_WRITE_CHAR_EVT: case ESP_GATTC_PREP_WRITE_EVT:
        e.status = p->write.status; e.a = p->write.conn_id; e.b = p->write.handle; e.c = p->write.offset; break;
    case ESP_GATTC_EXEC_EVT: e.status = p->exec_cmpl.status; e.a = p->exec_cmpl.conn_id; break;
    case ESP_GATTC_NOTIFY_EVT:
        e.a = p->notify.conn_id; e.b = p->notify.handle; e.c = p->notify.is_notify; break;
    case ESP_GATTC_READ_CHAR_EVT:
        e.status = p->read.status; e.a = p->read.conn_id; e.b = p->read.handle; e.c = p->read.value_len;
        if (p->read.value && p->read.value_len <= MAX_FILE) std::memcpy(e.text, p->read.value, p->read.value_len);
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        e.a = p->disconnect.conn_id; e.status = p->disconnect.reason;
        std::memcpy(e.address, p->disconnect.remote_bda, sizeof(e.address)); break;
    case ESP_GATTC_SRVC_CHG_EVT:
        std::memcpy(e.address, p->srvc_chg.remote_bda, sizeof(e.address)); break;
    default: return;
    }
    post(e);
}
void classic_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *p) {
    Event e{}; e.kind = Kind::classic; e.code = event;
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT:
        std::memcpy(e.address, p->disc_res.bda, sizeof(e.address));
        for (int i = 0; i < p->disc_res.num_prop; ++i) {
            auto &prop = p->disc_res.prop[i];
            if (prop.type == ESP_BT_GAP_DEV_PROP_BDNAME && prop.val) {
                std::memcpy(e.text, prop.val, std::min<size_t>(prop.len, sizeof(e.text) - 1));
            } else if (prop.type == ESP_BT_GAP_DEV_PROP_EIR && prop.val) {
                uint8_t len = 0;
                uint8_t *name = esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(prop.val), ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
                if (!name) name = esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(prop.val), ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
                if (name) { std::memset(e.text, 0, sizeof(e.text)); std::memcpy(e.text, name, std::min<size_t>(len, sizeof(e.text) - 1)); }
            }
        }
        break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: e.a = p->disc_st_chg.state; break;
    case ESP_BT_GAP_PIN_REQ_EVT: std::memcpy(e.address, p->pin_req.bda, 6); e.a = p->pin_req.min_16_digit; break;
    case ESP_BT_GAP_CFM_REQ_EVT: std::memcpy(e.address, p->cfm_req.bda, 6); e.a = p->cfm_req.num_val; break;
    case ESP_BT_GAP_AUTH_CMPL_EVT: std::memcpy(e.address, p->auth_cmpl.bda, 6); e.status = p->auth_cmpl.stat; break;
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
        std::memcpy(e.address, p->acl_conn_cmpl_stat.bda, 6);
        e.status = p->acl_conn_cmpl_stat.stat; e.a = p->acl_conn_cmpl_stat.handle; break;
    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
        std::memcpy(e.address, p->acl_disconn_cmpl_stat.bda, 6);
        e.status = p->acl_disconn_cmpl_stat.reason; e.a = p->acl_disconn_cmpl_stat.handle; break;
    default: return;
    }
    post(e);
}
void a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *p) {
    Event e{}; e.kind = Kind::a2dp; e.code = event;
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        e.a = p->conn_stat.state; std::memcpy(e.address, p->conn_stat.remote_bda, 6); break;
    case ESP_A2D_AUDIO_STATE_EVT: e.a = p->audio_stat.state; break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT: e.a = p->media_ctrl_stat.cmd; e.status = p->media_ctrl_stat.status; break;
    case ESP_A2D_AUDIO_CFG_EVT:
        e.a = p->audio_cfg.mcc.type;
        if (p->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
            e.sbc = p->audio_cfg.mcc.cie.sbc_info;
            e.b = e.sbc.samp_freq;
        }
        break;
    default: return;
    }
    post(e);
}
int32_t audio_callback(uint8_t *data, int32_t length) {
    ++callbacks;
    if (faulted.load()) {
        if (data && length > 0) std::memset(data, 0, static_cast<size_t>(length));
        return data && length > 0 ? length : 0;
    }
    int32_t count = pcm ? pcm(data, length) : 0;
    if (count > 0) bytes.fetch_add(static_cast<uint32_t>(count));
    return count;
}
void prepare_next() {
    const size_t total = std::strlen(pending);
    if (prepared >= total) {
        esp_err_t error = esp_ble_gattc_execute_write(iface(), connection, true);
        if (error != ESP_OK) { failed("execute filename", error); finish(PeerResult::rejected); }
        return;
    }
    preparing = std::min<size_t>(mtu - 5, total - prepared);
    esp_err_t e = esp_ble_gattc_prepare_write(iface(), connection, characteristic, prepared, preparing,
        reinterpret_cast<uint8_t *>(pending + prepared), ESP_GATT_AUTH_REQ_NONE);
    if (e != ESP_OK) { failed("prepare filename", e); finish(PeerResult::rejected); }
}
void handle_gatt(const Event &e) {
    if (e.code == ESP_GATTC_REG_EVT) {
        if (e.status == ESP_GATT_OK) {
            own_iface.store(e.iface);
            ESP_LOGI(TAG, "GATTC registered interface=%d; starting speaker discovery", e.iface);
            seek();
        }
        else ESP_LOGE(TAG, "GATTC registration failed %d", e.status);
        return;
    }
    if (e.code == ESP_GATTC_OPEN_EVT) {
        if (!opening || !same(e.address, peer_address)) return;
        opening = false;
        if (e.status != ESP_GATT_OK) { abandon_open = false; setup_deadline = 0; ESP_LOGW(TAG, "Jeff open failed %d", e.status); return; }
        have_peer = true; connection = e.a; mtu = std::max(23, e.b);
        if (abandon_open) { abandon_open = false; close_peer(); return; }
        service_start = service_end = characteristic = descriptor = 0;
        failed("MTU request", esp_ble_gattc_send_mtu_req(iface(), connection));
        return;
    }
    if (e.code == ESP_GATTC_REG_FOR_NOTIFY_EVT) {
        if (!have_peer || e.b != characteristic) return;
        esp_gattc_descr_elem_t element{}; uint16_t count = 1;
        if (e.status != ESP_GATT_OK || esp_ble_gattc_get_descr_by_char_handle(iface(), connection, characteristic, CCCD, &element, &count) != ESP_GATT_OK || count != 1) { close_peer(); return; }
        descriptor = element.handle;
        // Despite canIndicate(), the legacy client calls registerForNotify()
        // with its default notifications=true. Jeff calls notify(), whose
        // Arduino implementation requires the CCCD notification bit (not 2).
        uint8_t value[2] = {1, 0};
        if (esp_ble_gattc_write_char_descr(iface(), connection, descriptor, 2, value, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE) != ESP_OK) close_peer();
        return;
    }
    if (e.code == ESP_GATTC_SRVC_CHG_EVT) { if (have_peer && same(e.address, peer_address)) { finish(PeerResult::disconnected); close_peer(); } return; }
    if (!have_peer || e.a != connection) return;
    if (closing && e.code != ESP_GATTC_DISCONNECT_EVT) return;
    switch (e.code) {
    case ESP_GATTC_CFG_MTU_EVT: if (e.status == ESP_GATT_OK) mtu = std::max(23, e.b); break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (e.status != ESP_GATT_OK || esp_ble_gattc_search_service(iface(), connection, const_cast<esp_bt_uuid_t *>(&SERVICE)) != ESP_OK) close_peer();
        break;
    case ESP_GATTC_SEARCH_RES_EVT: service_start = e.b; service_end = e.c; break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
        esp_gattc_char_elem_t element{}; uint16_t count = 1;
        if (e.status != ESP_GATT_OK || !service_start || esp_ble_gattc_get_char_by_uuid(iface(), connection, service_start, service_end, CHARACTERISTIC, &element, &count) != ESP_GATT_OK || count != 1 || !(element.properties & ESP_GATT_CHAR_PROP_BIT_INDICATE) || !(element.properties & ESP_GATT_CHAR_PROP_BIT_READ) || !(element.properties & ESP_GATT_CHAR_PROP_BIT_WRITE)) { close_peer(); break; }
        characteristic = element.char_handle;
        if (esp_ble_gattc_register_for_notify(iface(), peer_address, characteristic) != ESP_OK) close_peer();
        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (e.b != descriptor) break;
        if (e.status != ESP_GATT_OK) { close_peer(); break; }
        initial_read = true;
        if (esp_ble_gattc_read_char(iface(), connection, characteristic, ESP_GATT_AUTH_REQ_NONE) != ESP_OK) close_peer();
        break;
    case ESP_GATTC_READ_CHAR_EVT:
        if (e.b != characteristic) break;
        if (initial_read) {
            initial_read = false;
            if (e.status != ESP_GATT_OK) { close_peer(); break; }
            // A previous accepted filename may remain, so no greeting-value requirement.
            setup_deadline = 0; peer_up.store(true);
            ESP_LOGI(TAG, "Jeff ready; initial value length=%d, MTU=%u", e.c, mtu);
        } else if (result.load() == PeerResult::pending && read_issued) {
            finish(e.status == ESP_GATT_OK && e.c == static_cast<int>(std::strlen(pending)) && !std::memcmp(e.text, pending, e.c) ? PeerResult::accepted : PeerResult::rejected);
        }
        break;
    case ESP_GATTC_NOTIFY_EVT:
        if (e.b == characteristic && result.load() == PeerResult::pending && !notification_seen) {
            notification_seen = true; read_at = now() + 100000;
            ESP_LOGI(TAG, "Jeff %s received; readback follows in 100ms", e.c ? "notification" : "indication");
        }
        break;
    case ESP_GATTC_PREP_WRITE_EVT:
        if (result.load() != PeerResult::pending || !long_write || e.b != characteristic || e.c != prepared) break;
        if (e.status != ESP_GATT_OK) { esp_ble_gattc_execute_write(iface(), connection, false); finish(PeerResult::rejected); }
        else { prepared += preparing; prepare_next(); }
        break;
    case ESP_GATTC_EXEC_EVT: case ESP_GATTC_WRITE_CHAR_EVT:
        if (e.status != ESP_GATT_OK) finish(PeerResult::rejected);
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        if (!same(e.address, peer_address)) break;
        ESP_LOGW(TAG, "Jeff disconnected reason=%d", e.status);
        have_peer = opening = initial_read = closing = false; setup_deadline = close_deadline = 0;
        peer_up.store(false); finish(PeerResult::disconnected);
        break;
    default: break;
    }
}
void handle_classic(const Event &e) {
    switch (e.code) {
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
        if (!same(e.address, speaker_address)) break;
        ESP_LOGI(TAG, "Configured speaker ACL complete status=%d handle=%d", e.status, e.a);
        if (e.status == ESP_BT_STATUS_SUCCESS) {
            speaker_acl_up = true; speaker_acl_handle = e.a;
        } // A failed second connection must not erase an already-live ACL.
        break;
    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
        if (!same(e.address, speaker_address)) break;
        ESP_LOGI(TAG, "Configured speaker ACL disconnected reason=%d handle=%d", e.status, e.a);
        if (speaker_acl_up && e.a != speaker_acl_handle) break;
        speaker_acl_up = false;
        if (await_acl_down) {
            await_acl_down = false; acl_wait_deadline = 0;
            schedule_speaker_retry();
        }
        break;
    case ESP_BT_GAP_DISC_RES_EVT:
        ESP_LOGI(TAG, "Classic candidate %02x:%02x:%02x:%02x:%02x:%02x name='%s'",
                 e.address[0], e.address[1], e.address[2], e.address[3], e.address[4], e.address[5], e.text);
        if (inquiry && !found_speaker && speaker_name[0] && !std::strcmp(e.text, speaker_name)) {
            saved_speaker = known_speaker && saved_speaker && same(e.address, speaker_address);
            known_speaker = true;
            found_speaker = true; std::memcpy(speaker_address, e.address, 6);
            failed("cancel speaker inquiry", esp_bt_gap_cancel_discovery());
            ESP_LOGI(TAG, "Exact configured speaker found: %s", e.text);
        }
        break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        inquiry = e.a == ESP_BT_GAP_DISCOVERY_STARTED;
        ESP_LOGI(TAG, "Speaker inquiry %s; configured match=%d", inquiry ? "started" : "stopped", found_speaker);
        if (!inquiry && found_speaker && !speaker_up.load() && !speaker_connecting &&
            !speaker_acl_up && !await_acl_down) {
            reconnect_attempts = 0;
            esp_err_t error = esp_a2d_source_connect(speaker_address);
            speaker_connecting = error == ESP_OK; failed("speaker connect", error);
            if (speaker_connecting) speaker_connect_deadline = now() + 75000000;
            else schedule_speaker_retry(true);
        } else if (!inquiry && !speaker_up.load() && !speaker_connecting) {
            schedule_speaker_retry(true);
            ESP_LOGI(TAG, "Speaker unavailable; next connection episode in 5 seconds (keeps retrying)");
        }
        break;
    case ESP_BT_GAP_PIN_REQ_EVT: {
        const bool allow = (speaker_connecting || speaker_up.load()) && same(e.address, speaker_address) && !e.a;
        esp_bt_pin_code_t pin = {'0','0','0','0'};
        ESP_LOGI(TAG, "Legacy PIN requested: %s", allow ? "configured speaker, replying 0000" : "declined");
        failed("PIN reply", esp_bt_gap_pin_reply(const_cast<uint8_t *>(e.address), allow, allow ? 4 : 0, pin));
        break;
    }
    case ESP_BT_GAP_CFM_REQ_EVT: {
        const bool allow = (speaker_connecting || speaker_up.load()) && same(e.address, speaker_address);
        ESP_LOGI(TAG, "Just Works confirmation: %s", allow ? "configured speaker consent" : "declined");
        failed("SSP confirmation", esp_bt_gap_ssp_confirm_reply(const_cast<uint8_t *>(e.address), allow));
        break;
    }
    case ESP_BT_GAP_AUTH_CMPL_EVT: ESP_LOGI(TAG, "Classic authentication status=%d", e.status); break;
    default: break;
    }
}
void try_media_start() {
    if (!speaker_up.load() || media_ready.load()) { media_retry_at = 0; return; }
    media_attempts = std::min(media_attempts + 1, 3u);
    failed("A2DP readiness", esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY));
    media_retry_at = now() + (media_attempts >= 3 ? 5000000 : 2000000);
}
void handle_a2dp(const Event &e) {
    if (e.code == ESP_A2D_CONNECTION_STATE_EVT) {
        if (!same(e.address, speaker_address)) {
            // This PCB used to be Jeff: his remembered speaker may initiate
            // reconnection even though this card selects Anthony's speaker.
            ESP_LOGW(TAG, "Unselected A2DP peer %02x:%02x:%02x:%02x:%02x:%02x state=%d; declining",
                     e.address[0], e.address[1], e.address[2], e.address[3], e.address[4], e.address[5], e.a);
            if (e.a == ESP_A2D_CONNECTION_STATE_CONNECTED || e.a == ESP_A2D_CONNECTION_STATE_CONNECTING)
                failed("disconnect unselected speaker", esp_a2d_source_disconnect(const_cast<uint8_t *>(e.address)));
            return;
        }
        ESP_LOGI(TAG, "A2DP connection state=%d", e.a);
        if (e.a == ESP_A2D_CONNECTION_STATE_CONNECTING) {
            speaker_connecting = true;
            if (!speaker_connect_deadline) speaker_connect_deadline = now() + 75000000;
        } else if (e.a == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            speaker_connect_deadline = 0;
            await_acl_down = false; acl_wait_deadline = 0;
            reconnect_attempts = 0; // A successful session starts a fresh recovery budget.
            reconnect_at = 0;
            speaker_connecting = false; speaker_up.store(true);
            save_speaker(); // Worker only, before starting PCM; never in callback.
            media_attempts = 0; try_media_start();
        } else if (e.a == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            speaker_connect_deadline = 0;
            speaker_up.store(false); speaker_connecting = streaming = false;
            media_ready.store(false); media_retry_at = 0;
            if (speaker_acl_up) {
                await_acl_down = true;
                acl_wait_deadline = now() + 60000000;
                ESP_LOGI(TAG, "Waiting for selected speaker ACL teardown before retry");
            } else {
                await_acl_down = false; acl_wait_deadline = 0;
                // A failed discovery connection starts another complete episode;
                // cached failures advance to inquiry after the third attempt.
                schedule_speaker_retry();
                ESP_LOGI(TAG, "Speaker disconnected; retry in 5 seconds");
            }
        }
    } else if (e.code == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
        ESP_LOGI(TAG, "A2DP media ack cmd=%d status=%d", e.a, e.status);
        if (e.a == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY && e.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS && speaker_up.load()) {
            failed("A2DP start", esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START));
        }
        if (e.a == ESP_A2D_MEDIA_CTRL_START && e.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
            streaming = true; media_ready.store(true); media_retry_at = 0;
        }
    } else if (e.code == ESP_A2D_AUDIO_STATE_EVT) {
        const bool was_streaming = streaming;
        streaming = e.a == ESP_A2D_AUDIO_STATE_STARTED;
        media_ready.store(streaming);
        if (streaming) media_retry_at = 0;
        else if (was_streaming && speaker_up.load()) { media_attempts = 0; media_retry_at = now() + 1000000; }
        ESP_LOGI(TAG, "A2DP audio state=%d", e.a);
    } else if (e.code == ESP_A2D_AUDIO_CFG_EVT) {
        ESP_LOGI(TAG, "A2DP codec=%d SBC frequency mask=%d (PCM contract 44100/16/stereo)", e.a, e.b);
        if (e.a == ESP_A2D_MCT_SBC) {
            static_assert(sizeof(e.sbc) == 4, "SBC codec information must contain four bytes");
            const auto *raw = reinterpret_cast<const uint8_t *>(&e.sbc);
            ESP_LOGI(TAG, "SBC negotiated raw=%02x:%02x:%02x:%02x frequency_mask=0x%x channel_mask=0x%x block_mask=0x%x subbands_mask=0x%x allocation_mask=0x%x bitpool=%u..%u",
                     raw[0], raw[1], raw[2], raw[3], e.sbc.samp_freq, e.sbc.ch_mode,
                     e.sbc.block_len, e.sbc.num_subbands, e.sbc.alloc_mthd,
                     e.sbc.min_bitpool, e.sbc.max_bitpool);
        }
    }
}
void worker(void *) {
    Event e{};
    for (;;) {
        if (xQueueReceive(queue, &e, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (faulted.load()) continue;
            switch (e.kind) {
            case Kind::start: {
                primary_role = e.a != 0;
                load_speaker();
                ESP_LOGI(TAG, "Radio start: configured speaker='%s', peer selection=%s", speaker_name,
                         fixed_peer ? "configured address" : "exact legacy name");
                failed("local MTU", esp_ble_gatt_set_local_mtu(247));
                if (primary_role) failed("GATTC register", esp_ble_gattc_app_register(APP_ID));
                else {
                    failed("secondary BT name", esp_bt_dev_set_device_name("SkullSecondary"));
                    seek(); // Standalone server does not need a GATTC interface.
                }
                // Primary inquiry starts after its GATTC REG_EVT.
                break;
            }
            case Kind::retry:
                ESP_LOGI(TAG, "Explicit idle discovery/media retry");
                if (speaker_acl_up && !speaker_up.load() && !speaker_connecting && !await_acl_down) {
                    await_acl_down = true; acl_wait_deadline = now() + 60000000;
                }
                if (!speaker_connecting && !await_acl_down && !reconnect_at) {
                    reconnect_attempts = 0; reconnect_at = 0;
                } else if (speaker_connecting || await_acl_down || reconnect_at) {
                    ESP_LOGW(TAG, "Retry held for prior connection/ACL teardown or active backoff");
                }
                if (speaker_up.load() && !media_ready.load()) { media_attempts = 0; try_media_start(); }
                seek(); break;
            case Kind::peer_discovery:
                peer_discovery_enabled = primary_role && e.a != 0;
                ESP_LOGI(TAG, "Idle Jeff discovery %s", peer_discovery_enabled ? "enabled" : "disabled");
                if (!peer_discovery_enabled) stop_scan();
                else seek_peer();
                break;
            case Kind::request: {
                if (!peer_up.load()) { finish(PeerResult::disconnected); break; }
                std::strcpy(pending, e.text);
                notification_seen = read_issued = false; deadline = now() + 5000000; read_at = 0;
                ++requests;
                const size_t length = std::strlen(pending);
                long_write = length > static_cast<size_t>(mtu - 3);
                if (long_write) { prepared = preparing = 0; prepare_next(); }
                else {
                    // Arduino BLE 2.0.13 writeValue(std::string, response=false).
                    esp_err_t error = esp_ble_gattc_write_char(iface(), connection, characteristic, length,
                        reinterpret_cast<uint8_t *>(pending), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
                    if (error != ESP_OK) { failed("filename write", error); finish(PeerResult::rejected); }
                }
                break;
            }
            case Kind::ble_gap:
                if (e.code == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
                    scan_setting = false; scan_configured = e.status == ESP_BT_STATUS_SUCCESS;
                    if (scan_configured) seek_peer();
                    else scan_backoff(true);
                } else if (e.code == ESP_GAP_BLE_SCAN_START_COMPLETE_EVT) {
                    scan_starting = false;
                    scanning = e.status == ESP_BT_STATUS_SUCCESS;
                    scan_active.store(scanning);
                    if (!scanning) scan_backoff(true);
                    else if (!peer_discovery_enabled) stop_scan();
                } else if (e.code == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT) {
                    scan_stopping = false;
                    if (e.status == ESP_BT_STATUS_SUCCESS) {
                        scanning = false; scan_active.store(false); scan_backoff(false);
                    } else scan_backoff(true);
                } else if (e.code == ESP_GAP_BLE_SCAN_RESULT_EVT) {
                    if (e.a == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
                        scanning = scan_starting = scan_stopping = false;
                        scan_active.store(false); scan_backoff(false);
                    }
                    else if (e.a == ESP_GAP_SEARCH_INQ_RES_EVT && peer_discovery_enabled && scanning && !scan_stopping && !have_peer && !opening &&
                        (fixed_peer ? same(selected_peer, e.address) : !std::strcmp(e.text, SECONDARY_NAME))) {
                        stop_scan(); std::memcpy(peer_address, e.address, 6);
                        opening = true; abandon_open = false; setup_deadline = now() + 15000000;
                        esp_err_t error = esp_ble_gattc_open(iface(), peer_address, static_cast<esp_ble_addr_type_t>(e.b), true);
                        if (error != ESP_OK) { opening = false; setup_deadline = 0; failed("open Jeff", error); }
                    }
                }
                break;
            case Kind::gatt: handle_gatt(e); break;
            case Kind::classic: handle_classic(e); break;
            case Kind::a2dp: handle_a2dp(e); break;
            }
        }
        if (overflow.exchange(false)) {
            // We cannot infer which control transition was lost. Latch a fault,
            // silence application audio and require a deliberate reboot.
            ESP_LOGE(TAG, "Radio queue overflow: gate FAILED; reboot required");
            finish(PeerResult::disconnected);
            peer_up.store(false); speaker_up.store(false); media_ready.store(false);
            faulted.store(true);
        }
        if (faulted.load()) continue;
        const int64_t time = now();
        if (speaker_connect_deadline && time >= speaker_connect_deadline) {
            // The pinned SDK does not support cancelling A2DP while OPENING.
            // Keep observing until completion; never start an overlapping open.
            speaker_connect_deadline = time + 75000000;
            ESP_LOGW(TAG, "Speaker connection still pending; waiting for stack completion before retry");
        }
        if (acl_wait_deadline && time >= acl_wait_deadline) {
            acl_wait_deadline = time + 60000000;
            ESP_LOGW(TAG, "Speaker ACL teardown still pending; automatic retry resumes when released");
        }
        if (reconnect_at && time >= reconnect_at) { reconnect_at = 0; seek(); }
        if (close_deadline && time >= close_deadline) {
            ESP_LOGE(TAG, "Peer teardown did not complete: reboot required");
            faulted.store(true); peer_up.store(false); media_ready.store(false);
            continue;
        }
        // Classic retries run only from their deadline or completion events;
        // polling BLE must not bypass the speaker backoff.
        if (!peer_discovery_enabled && scanning && !scan_stopping &&
            (!scan_retry_at || time >= scan_retry_at)) stop_scan();
        seek_peer();
        if (media_retry_at && time >= media_retry_at) try_media_start();
        if (setup_deadline && time >= setup_deadline) { ESP_LOGW(TAG, "Jeff setup timed out"); close_peer(); }
        if (result.load() == PeerResult::pending && deadline) {
            if (time >= deadline) {
                if (long_write) esp_ble_gattc_execute_write(iface(), connection, false);
                finish(PeerResult::timed_out);
            } else if (notification_seen && !read_issued && time >= read_at) {
                read_issued = true;
                esp_err_t error = esp_ble_gattc_read_char(iface(), connection, characteristic, ESP_GATT_AUTH_REQ_NONE);
                if (error != ESP_OK) { failed("ACK readback", error); finish(PeerResult::rejected); }
            }
        }
    }
}
}

esp_err_t init(PcmCallback callback) {
    if (initialized.load()) return ESP_ERR_INVALID_STATE;
    if (!callback) return ESP_ERR_INVALID_ARG;
    // Deliberately retain both BLE and Classic controller/host memory.
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t error = esp_bt_controller_init(&cfg);
    if (error != ESP_OK) return error;
    if ((error = esp_bt_controller_enable(ESP_BT_MODE_BTDM)) != ESP_OK) return error;
    if ((error = esp_bluedroid_init()) != ESP_OK) return error;
    if ((error = esp_bluedroid_enable()) != ESP_OK) return error;
    pcm = callback;
    // Only task callbacks use this copied-event queue. Keep scarce internal
    // memory for controller, stacks and DMA; no ISR may use this PSRAM queue.
    auto *queue_storage = static_cast<uint8_t *>(heap_caps_malloc(32 * sizeof(Event), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!queue_storage) return ESP_ERR_NO_MEM;
    queue = xQueueCreateStatic(32, sizeof(Event), queue_storage, &queue_control);
    if (!queue) { heap_caps_free(queue_storage); return ESP_ERR_NO_MEM; }
    if (xTaskCreate(worker, "skull_radio", 6144, nullptr, 5, &worker_task) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "Radio copied-event queue: %u bytes in PSRAM; worker stack internal",
             static_cast<unsigned>(32 * sizeof(Event)));
    if ((error = esp_bt_gap_register_callback(classic_callback)) != ESP_OK) return error;
    if ((error = esp_ble_gattc_register_callback(gatt_callback)) != ESP_OK) return error;
    if ((error = esp_a2d_register_callback(a2dp_callback)) != ESP_OK) return error;
    if ((error = esp_a2d_source_register_data_callback(audio_callback)) != ESP_OK) return error;
    if ((error = esp_a2d_source_init()) != ESP_OK) return error;
    // Explicitly advertise only 44.1kHz stereo, matching parent's signed 16-bit PCM.
    esp_a2d_mcc_t codec{}; codec.type = ESP_A2D_MCT_SBC;
    codec.cie.sbc_info.samp_freq = ESP_A2D_SBC_CIE_SF_44K;
    codec.cie.sbc_info.ch_mode = ESP_A2D_SBC_CIE_CH_MODE_STEREO;
    codec.cie.sbc_info.block_len = ESP_A2D_SBC_CIE_BLOCK_LEN_16;
    codec.cie.sbc_info.num_subbands = ESP_A2D_SBC_CIE_NUM_SUBBANDS_8;
    codec.cie.sbc_info.alloc_mthd = ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS;
    codec.cie.sbc_info.min_bitpool = 2; codec.cie.sbc_info.max_bitpool = 53;
    if ((error = esp_a2d_source_register_stream_endpoint(0, &codec)) != ESP_OK) return error;
    uint8_t capability = ESP_BT_IO_CAP_NONE;
    if ((error = esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &capability, sizeof(capability))) != ESP_OK) return error;
    failed("BT name", esp_bt_dev_set_device_name("SkullPrimary-Gate1"));
    // Initiate the configured speaker connection ourselves; don't let the old
    // physical board's remembered speaker seize the only A2DP profile first.
    failed("Classic outgoing-only", esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));
    initialized.store(true);
    return ESP_OK;
}
esp_err_t attach_matter_gap() {
    if (standalone_gap.load()) return ESP_ERR_INVALID_STATE;
    if (!initialized.load()) return ESP_ERR_INVALID_STATE;
    auto current = esp_ble_gap_get_callback();
    if (current == gap_dispatch) return attached.load() ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (!current) return ESP_ERR_NOT_FOUND;
    matter_gap = current;
    esp_err_t error = esp_ble_gap_register_callback(gap_dispatch);
    if (error == ESP_OK) attached.store(true);
    return error;
}
esp_err_t attach_standalone_gap() {
    if (!initialized.load()) return ESP_ERR_INVALID_STATE;
    auto current = esp_ble_gap_get_callback();
    if (current == gap_dispatch && standalone_gap.load()) return ESP_OK;
    if (current) return ESP_ERR_INVALID_STATE; // Never steal Matter's callback.
    matter_gap = nullptr;
    standalone_gap.store(true);
    const esp_err_t error = esp_ble_gap_register_callback(gap_dispatch);
    if (error == ESP_OK) attached.store(true);
    else standalone_gap.store(false);
    return error;
}
esp_err_t start(const Config &config) {
    if (!initialized.load() || !attached.load()) return ESP_ERR_INVALID_STATE;
    if (config.speaker_name && std::strlen(config.speaker_name) >= sizeof(speaker_name)) return ESP_ERR_INVALID_ARG;
    bool expected = false;
    if (!started.compare_exchange_strong(expected, true)) return ESP_ERR_INVALID_STATE;
    if (config.speaker_name) std::strcpy(speaker_name, config.speaker_name);
    if (config.secondary_mac && config.secondary_mac[0]) {
        unsigned values[6]; char tail;
        if (std::sscanf(config.secondary_mac, "%2x:%2x:%2x:%2x:%2x:%2x%c", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5], &tail) == 6) {
            for (int i = 0; i < 6; ++i) selected_peer[i] = values[i];
            fixed_peer = true;
        } else ESP_LOGW(TAG, "Invalid secondary MAC: using exact service name selection");
    }
    Event e{}; e.kind = Kind::start; e.a = config.primary ? 1 : 0;
    if (!post(e)) { started.store(false); return ESP_ERR_NO_MEM; }
    return ESP_OK;
}
bool speaker_connected() { return !faulted.load() && speaker_up.load(); }
bool stream_ready() { return !faulted.load() && speaker_up.load() && media_ready.load(); }
bool peer_ready() { return !faulted.load() && peer_up.load(); }
void set_peer_discovery(bool enabled) {
    if (!initialized.load()) return;
    Event e{}; e.kind = Kind::peer_discovery; e.a = enabled ? 1 : 0;
    post(e); // Queue overflow follows the existing fail-closed radio policy.
}
bool peer_scanning() { return !faulted.load() && scan_active.load(); }
esp_err_t request_file(const char *filename) {
    if (!filename || !filename[0] || strnlen(filename, MAX_FILE + 1) > MAX_FILE) return ESP_ERR_INVALID_ARG;
    if (faulted.load() || !started.load() || !peer_up.load()) return ESP_ERR_INVALID_STATE;
    PeerResult expected = PeerResult::idle;
    if (!result.compare_exchange_strong(expected, PeerResult::pending)) return ESP_ERR_INVALID_STATE;
    Event e{}; e.kind = Kind::request; std::strcpy(e.text, filename);
    if (!post(e)) { result.store(PeerResult::idle); return ESP_ERR_NO_MEM; }
    return ESP_OK;
}
PeerResult peer_result() { return result.load(); }
void clear_peer_result() {
    auto value = result.load();
    while (value != PeerResult::pending && value != PeerResult::idle && !result.compare_exchange_weak(value, PeerResult::idle)) {}
}
Stats stats() { return {callbacks.load(), bytes.load(), drops.load(), requests.load(), accepted.load(), faulted.load(),
    worker_task ? static_cast<uint32_t>(uxTaskGetStackHighWaterMark(worker_task)) : 0}; }
void retry_connections() { if (started.load()) { Event e{}; e.kind = Kind::retry; post(e); } }
}
