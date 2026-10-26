#include "skull_secondary.h"
#include "skull_secondary_protocol.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
namespace skull::secondary {
namespace {
constexpr uint16_t app_id = 0x534c;
constexpr int slots = 16;
constexpr int64_t timeout_us = 3000000;
enum Attribute { service, declaration, value, cccd, attr_count };
const uint8_t service_uuid[16] = {0x4b,0x91,0x31,0xc3,0xc9,0xc5,0xcc,0x8f,0x9e,0x45,0xb5,0x1f,0x01,0xc2,0xaf,0x4f};
const uint8_t char_uuid[16] = {0xa8,0x26,0x1b,0x36,0x07,0xea,0xf5,0xb7,0x88,0x46,0xe1,0x36,0x3e,0x48,0xb5,0xbe};
const uint16_t primary_uuid = ESP_GATT_UUID_PRI_SERVICE, declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE, cccd_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
const uint8_t properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_INDICATE;
uint8_t greeting[] = "Hello from SkullSecondary", initial_cccd[2]{};
#define MUTABLE(p) const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(p))
const esp_gatts_attr_db_t table[attr_count] = {
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, MUTABLE(&primary_uuid), ESP_GATT_PERM_READ, 16, 16, MUTABLE(service_uuid)}},
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, MUTABLE(&declaration_uuid), ESP_GATT_PERM_READ, 1, 1, MUTABLE(&properties)}},
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_128, MUTABLE(char_uuid), ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 259, sizeof(greeting)-1, greeting}},
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, MUTABLE(&cccd_uuid), ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 2, initial_cccd}}
};
#undef MUTABLE
enum class Kind { registration, table, start, connect, disconnect, read, write, execute, mtu, gap };
struct Event {
    Kind kind{};
    uint32_t generation{}, transaction{};
    uint16_t conn{}, handle{}, offset{}, length{}, handles[attr_count]{};
    int status{}, code{};
    bool need_response{}, prepared{};
    uint8_t bytes[protocol::max_path]{};
};
QueueHandle_t queue{};
StaticQueue_t queue_control;
std::atomic<int> callback_iface{ESP_GATT_IF_NONE}, callback_conn{-1};
std::atomic<uint32_t> callback_generation{0};
std::atomic<bool> overflow{false};
// Below is exclusively owned by the control task (including init).
bool initialized{}, service_ready{}, link{}, connected_edge{}, blocked{true}, wanted_advertising{}, advertising_now{}, advertising_starting{}, advertising_stopping{}, adv_configured{}, rsp_configured{}, pending_taken{};
uint16_t handles[attr_count]{}, conn{}, mtu{23}, subscription{};
uint32_t generation{};
int64_t request_deadline{}, assembly_deadline{}, adv_retry_at{};
protocol::Assembly assembly;
protocol::Ticket ticket;
Request pending{};
char readable[260] = "Hello from SkullSecondary";
esp_gatt_if_t iface() { return static_cast<esp_gatt_if_t>(callback_iface.load()); }
int64_t now() { return esp_timer_get_time(); }
void post(const Event &e) {
    if (!queue || xQueueSend(queue, &e, 0) != pdTRUE) overflow.store(true);
}
void callback(esp_gatts_cb_event_t type, esp_gatt_if_t gi, esp_ble_gatts_cb_param_t *p) {
    Event e{};
    if (type == ESP_GATTS_REG_EVT) {
        if (p->reg.app_id != app_id) return;
        if (p->reg.status == ESP_GATT_OK) callback_iface.store(gi);
        e.kind = Kind::registration; e.status = p->reg.status;
    } else {
        if (gi == ESP_GATT_IF_NONE || gi != callback_iface.load()) return;
        switch (type) {
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            e.kind = Kind::table; e.status = p->add_attr_tab.status; e.length = p->add_attr_tab.num_handle;
            if (e.length == attr_count) std::memcpy(e.handles, p->add_attr_tab.handles, sizeof(e.handles));
            break;
        case ESP_GATTS_START_EVT: e.kind = Kind::start; e.status = p->start.status; break;
        case ESP_GATTS_CONNECT_EVT: {
            e.kind = Kind::connect; e.conn = p->connect.conn_id;
            int empty = -1;
            if (callback_conn.compare_exchange_strong(empty, e.conn)) e.generation = ++callback_generation;
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT:
            e.kind = Kind::disconnect; e.conn = p->disconnect.conn_id;
            if (callback_conn.load() == e.conn) { callback_conn.store(-1); e.generation = ++callback_generation; }
            break;
        case ESP_GATTS_MTU_EVT: e.kind = Kind::mtu; e.conn = p->mtu.conn_id; e.length = p->mtu.mtu; break;
        case ESP_GATTS_READ_EVT:
            e.kind = Kind::read; e.conn = p->read.conn_id; e.transaction = p->read.trans_id;
            e.handle = p->read.handle; e.offset = p->read.offset; e.need_response = p->read.need_rsp; break;
        case ESP_GATTS_WRITE_EVT:
            e.kind = Kind::write; e.conn = p->write.conn_id; e.transaction = p->write.trans_id;
            e.handle = p->write.handle; e.offset = p->write.offset; e.need_response = p->write.need_rsp;
            e.prepared = p->write.is_prep; e.length = p->write.len;
            if (e.length <= sizeof(e.bytes) && p->write.value) std::memcpy(e.bytes, p->write.value, e.length);
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            e.kind = Kind::execute; e.conn = p->exec_write.conn_id; e.transaction = p->exec_write.trans_id;
            e.code = p->exec_write.exec_write_flag; e.need_response = true; break;
        default: return;
        }
    }
    if (!e.generation) e.generation = callback_generation.load();
    post(e);
}
void reply(const Event &e, esp_gatt_status_t status, const uint8_t *bytes = nullptr, size_t length = 0) {
    if (!e.need_response) return;
    esp_gatt_rsp_t rsp{};
    rsp.attr_value.handle = e.handle; rsp.attr_value.offset = e.offset;
    rsp.attr_value.len = std::min(length, sizeof(rsp.attr_value.value));
    if (bytes && rsp.attr_value.len) std::memcpy(rsp.attr_value.value, bytes, rsp.attr_value.len);
    esp_ble_gatts_send_response(iface(), e.conn, e.transaction, status, &rsp);
}
bool notify() {
    if (!link || !subscription || overflow.load() || callback_generation.load() != generation) return false;
    // Legacy READ|WRITE|INDICATE characteristic nevertheless sends notify().
    const size_t length = std::min(std::strlen(readable), static_cast<size_t>(mtu - 3));
    return esp_ble_gatts_send_indicate(iface(), conn, handles[value], length,
        reinterpret_cast<uint8_t *>(readable), false) == ESP_OK;
}
void clear_pending() { ticket.clear(); pending_taken = false; request_deadline = 0; pending = {}; }
void reject(const char *path) {
    if (ticket.pending) return; // A second write cannot overwrite the first transaction's readback.
    std::snprintf(readable, sizeof(readable), "Error: Cannot play %s", path ? path : "invalid request");
    notify();
}
bool offer(const uint8_t *bytes, size_t length) {
    if (!length || length > protocol::max_path || std::memchr(bytes, 0, length)) { reject(nullptr); return false; }
    char path[241]{}; std::memcpy(path, bytes, length);
    if (ticket.pending) return false;
    if (blocked || !subscription || overflow.load()) { reject(path); return false; }
    pending = {}; pending.token = ticket.issue(); std::memcpy(pending.path, path, length + 1);
    pending_taken = false; request_deadline = now() + timeout_us;
    return true;
}
void update_advertising() {
    if (!service_ready || !adv_configured || !rsp_configured || link || now() < adv_retry_at) return;
    if (overflow.load()) wanted_advertising = false;
    if (!wanted_advertising && advertising_now && !advertising_stopping) {
        if (esp_ble_gap_stop_advertising() == ESP_OK) advertising_stopping = true;
        else adv_retry_at = now() + 2000000;
    } else if (wanted_advertising && !advertising_now && !advertising_starting && !advertising_stopping) {
        esp_ble_adv_params_t p{}; p.adv_int_min = 0x100; p.adv_int_max = 0x180;
        p.adv_type = ADV_TYPE_IND; p.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
        p.channel_map = ADV_CHNL_ALL; p.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
        if (esp_ble_gap_start_advertising(&p) == ESP_OK) advertising_starting = true;
        else adv_retry_at = now() + 2000000;
    }
}
void configure_advertising() {
    // Flags + complete service UUID = 21 bytes; complete name separately = 23.
    uint8_t adv[21] = {2, ESP_BLE_AD_TYPE_FLAG, ESP_BLE_ADV_FLAG_GEN_DISC, 17, ESP_BLE_AD_TYPE_128SRV_CMPL};
    std::memcpy(adv + 5, service_uuid, 16);
    constexpr char name[] = "SkullSecondary-Server";
    uint8_t rsp[sizeof(name)+1]{}; rsp[0] = sizeof(name); rsp[1] = ESP_BLE_AD_TYPE_NAME_CMPL;
    std::memcpy(rsp + 2, name, sizeof(name)-1);
    if (esp_ble_gap_config_adv_data_raw(adv, sizeof(adv)) != ESP_OK ||
        esp_ble_gap_config_scan_rsp_data_raw(rsp, sizeof(rsp)) != ESP_OK) overflow.store(true);
}
void handle_write(const Event &e) {
    if (e.handle == handles[cccd]) {
        if (e.prepared || e.offset || e.length != 2 || e.bytes[1] || e.bytes[0] > 2) { reply(e, ESP_GATT_INVALID_ATTR_LEN); return; }
        subscription = e.bytes[0]; reply(e, ESP_GATT_OK); return;
    }
    if (e.handle != handles[value]) { reply(e, ESP_GATT_INVALID_HANDLE); return; }
    if (blocked || ticket.pending || overflow.load()) { reply(e, ESP_GATT_BUSY); if (!ticket.pending) reject(nullptr); return; }
    if (e.prepared) {
        if (!assembly.active) assembly_deadline = now() + timeout_us;
        const bool valid = e.length <= sizeof(e.bytes) && assembly.append(e.offset, e.bytes, e.length);
        if (!valid) { assembly.active = true; assembly.invalid = true; }
        reply(e, valid ? ESP_GATT_OK : ESP_GATT_INVALID_ATTR_LEN, valid ? e.bytes : nullptr, valid ? e.length : 0);
    } else {
        if (assembly.active || e.offset || e.length > sizeof(e.bytes)) { reply(e, ESP_GATT_INVALID_OFFSET); reject(nullptr); return; }
        const bool accepted = offer(e.bytes, e.length);
        reply(e, accepted ? ESP_GATT_OK : ESP_GATT_ERROR);
    }
}
} // namespace
esp_err_t init() {
    if (initialized) return ESP_ERR_INVALID_STATE;
    // A standalone server must never steal Matter's global callback.
    if (esp_ble_gatts_get_callback()) return ESP_ERR_INVALID_STATE;
    auto *storage = static_cast<uint8_t *>(heap_caps_malloc(slots * sizeof(Event), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!storage) return ESP_ERR_NO_MEM;
    queue = xQueueCreateStatic(slots, sizeof(Event), storage, &queue_control);
    if (!queue) { heap_caps_free(storage); return ESP_ERR_NO_MEM; }
    esp_err_t error = esp_ble_gatts_register_callback(callback);
    if (error != ESP_OK) return error;
    error = esp_ble_gatts_app_register(app_id);
    if (error == ESP_OK) initialized = true;
    return error;
}
void gap_event(esp_gap_ble_cb_event_t type, esp_ble_gap_cb_param_t *p) {
    if (!queue) return;
    Event e{}; e.kind = Kind::gap; e.code = type;
    switch (type) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: e.status = p->adv_data_raw_cmpl.status; break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT: e.status = p->scan_rsp_data_raw_cmpl.status; break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: e.status = p->adv_start_cmpl.status; break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: e.status = p->adv_stop_cmpl.status; break;
    default: return;
    }
    post(e);
}
void poll() {
    if (!queue) return;
    Event e{};
    for (int i = 0; i < slots && xQueueReceive(queue, &e, 0) == pdTRUE; ++i) {
        if (e.kind == Kind::registration) {
            if (e.status == ESP_GATT_OK) {
                if (esp_ble_gatts_create_attr_tab(table, iface(), attr_count, 0) != ESP_OK) overflow.store(true);
            } else overflow.store(true);
        } else if (e.kind == Kind::table) {
            if (e.status != ESP_GATT_OK || e.length != attr_count) { overflow.store(true); continue; }
            std::memcpy(handles, e.handles, sizeof(handles));
            if (esp_ble_gatts_start_service(handles[service]) != ESP_OK) overflow.store(true);
        } else if (e.kind == Kind::start) {
            service_ready = e.status == ESP_GATT_OK;
            if (service_ready) configure_advertising(); else overflow.store(true);
        } else if (e.kind == Kind::gap) {
            if (e.code == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT) {
                adv_configured = e.status == ESP_BT_STATUS_SUCCESS;
                if (!adv_configured) overflow.store(true);
            } else if (e.code == ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT) {
                rsp_configured = e.status == ESP_BT_STATUS_SUCCESS;
                if (!rsp_configured) overflow.store(true);
            }
            else if (e.code == ESP_GAP_BLE_ADV_START_COMPLETE_EVT) {
                advertising_starting = false; advertising_now = e.status == ESP_BT_STATUS_SUCCESS;
                if (!advertising_now) adv_retry_at = now() + 2000000;
            } else if (e.code == ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT) {
                advertising_stopping = false;
                if (e.status == ESP_BT_STATUS_SUCCESS) advertising_now = false;
                else adv_retry_at = now() + 2000000;
            }
        } else if (e.kind == Kind::connect) {
            if (link) { esp_ble_gatts_close(iface(), e.conn); continue; }
            link = true; connected_edge = true; conn = e.conn; generation = e.generation;
            advertising_now = advertising_starting = false; subscription = 0; mtu = 23;
            assembly.reset(); assembly_deadline = 0; clear_pending();
        } else if (e.kind == Kind::disconnect) {
            if (link && e.conn == conn) {
                link = connected_edge = false; generation = e.generation; subscription = 0;
                assembly.reset(); assembly_deadline = 0; clear_pending();
            }
        } else {
            // A disconnect may already have occurred in the Bluetooth callback
            // while its queued event still waits behind this ATT transaction.
            // Never reply using a recycled controller connection ID.
            if (!link || e.conn != conn || e.generation != generation ||
                e.generation != callback_generation.load() || callback_conn.load() != e.conn) continue;
            if (e.kind == Kind::mtu) mtu = std::max<uint16_t>(23, e.length);
            else if (e.kind == Kind::write) handle_write(e);
            else if (e.kind == Kind::read) {
                uint8_t cccd_value[2] = {static_cast<uint8_t>(subscription), 0};
                const uint8_t *data = reinterpret_cast<const uint8_t *>(readable);
                size_t length = std::strlen(readable);
                if (e.handle == handles[cccd]) { data = cccd_value; length = 2; }
                else if (e.handle != handles[value]) { reply(e, ESP_GATT_INVALID_HANDLE); continue; }
                if (e.offset > length) reply(e, ESP_GATT_INVALID_OFFSET);
                else reply(e, ESP_GATT_OK, data + e.offset, std::min(length-e.offset, static_cast<size_t>(mtu-1)));
            } else if (e.kind == Kind::execute) {
                if (e.code == ESP_GATT_PREP_WRITE_CANCEL) reply(e, ESP_GATT_OK);
                else if (e.code == ESP_GATT_PREP_WRITE_EXEC && assembly.complete())
                    reply(e, offer(assembly.data, assembly.length) ? ESP_GATT_OK : ESP_GATT_ERROR);
                else { reply(e, ESP_GATT_INVALID_ATTR_LEN); reject(nullptr); }
                assembly.reset(); assembly_deadline = 0;
            }
        }
    }
    if (assembly_deadline && now() >= assembly_deadline) { assembly.reset(); assembly_deadline = 0; reject(nullptr); }
    if (request_deadline && now() >= request_deadline) {
        const uint32_t token = pending.token; respond(token, false);
    }
    if (overflow.load()) { clear_pending(); assembly.reset(); wanted_advertising = false; }
    update_advertising();
}
bool ready() { return service_ready && !overflow.load(); }
void advertising(bool enabled) { wanted_advertising = enabled; update_advertising(); }
bool connected() { return link && callback_generation.load() == generation && !overflow.load(); }
bool take_connected() { const bool result = connected_edge && connected(); connected_edge = false; return result; }
void busy(bool state) { blocked = state; }
bool take_request(Request &request) {
    if (!connected() || !ticket.pending || pending_taken || now() >= request_deadline) return false;
    request = pending; pending_taken = true; return true;
}
bool respond(uint32_t token, bool accept) {
    if (!ticket.matches(token)) return false;
    if (!connected() || (accept && now() >= request_deadline)) { clear_pending(); return false; }
    if (accept) std::snprintf(readable, sizeof(readable), "%s", pending.path);
    else std::snprintf(readable, sizeof(readable), "Error: Cannot play %s", pending.path);
    const bool queued = notify();
    clear_pending();
    return queued;
}
} // namespace skull::secondary
