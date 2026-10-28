#pragma once
// Host-only fake SDK surface. Async events are driven explicitly by the test;
// successful API returns mean queued requests, never simulated connection proof.
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <cstdio>
#include <cstdarg>
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_BT_STATUS_SUCCESS=0, ESP_GATT_OK=0, ESP_A2D_MEDIA_CTRL_ACK_SUCCESS=0;
constexpr size_t ESP_BD_ADDR_LEN=6;
constexpr int BLE_ADDR_TYPE_PUBLIC=10;
constexpr int BLE_SCAN_DUPLICATE_DISABLE=11;
constexpr int BLE_SCAN_FILTER_ALLOW_ALL=12;
constexpr int BLE_SCAN_TYPE_ACTIVE=13;
constexpr int ESP_A2D_AUDIO_CFG_EVT=14;
constexpr int ESP_A2D_AUDIO_STATE_EVT=15;
constexpr int ESP_A2D_AUDIO_STATE_STARTED=16;
constexpr int ESP_A2D_CONNECTION_STATE_CONNECTED=17;
constexpr int ESP_A2D_CONNECTION_STATE_CONNECTING=18;
constexpr int ESP_A2D_CONNECTION_STATE_DISCONNECTED=19;
constexpr int ESP_A2D_CONNECTION_STATE_EVT=20;
constexpr int ESP_A2D_MCT_SBC=21;
constexpr int ESP_A2D_MEDIA_CTRL_ACK_EVT=22;
constexpr int ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY=24;
constexpr int ESP_A2D_MEDIA_CTRL_START=25;
constexpr int ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS=1;
constexpr int ESP_A2D_SBC_CIE_BLOCK_LEN_16=1;
constexpr int ESP_A2D_SBC_CIE_CH_MODE_STEREO=2;
constexpr int ESP_A2D_SBC_CIE_NUM_SUBBANDS_8=1;
constexpr int ESP_A2D_SBC_CIE_SF_44K=2;
constexpr int ESP_BLE_AD_TYPE_NAME_CMPL=32;
constexpr int ESP_BLE_AD_TYPE_NAME_SHORT=33;
constexpr int ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME=34;
constexpr int ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME=35;
constexpr int ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT=36;
constexpr int ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT=37;
constexpr int ESP_BT_GAP_AUTH_CMPL_EVT=38;
constexpr int ESP_BT_GAP_CFM_REQ_EVT=39;
constexpr int ESP_BT_GAP_DEV_PROP_BDNAME=40;
constexpr int ESP_BT_GAP_DEV_PROP_EIR=41;
constexpr int ESP_BT_GAP_DISCOVERY_STARTED=42;
constexpr int ESP_BT_GAP_DISC_RES_EVT=43;
constexpr int ESP_BT_GAP_DISC_STATE_CHANGED_EVT=44;
constexpr int ESP_BT_GAP_PIN_REQ_EVT=45;
constexpr int ESP_BT_INQ_MODE_GENERAL_INQUIRY=46;
constexpr int ESP_BT_IO_CAP_NONE=47;
constexpr int ESP_BT_MODE_BTDM=48;
constexpr int ESP_BT_NON_CONNECTABLE=49;
constexpr int ESP_BT_NON_DISCOVERABLE=50;
constexpr int ESP_BT_SP_IOCAP_MODE=51;
constexpr int ESP_ERR_INVALID_ARG=53;
constexpr int ESP_ERR_INVALID_STATE=54;
constexpr int ESP_ERR_NOT_FOUND=55;
constexpr int ESP_ERR_NO_MEM=56;
constexpr int ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT=57;
constexpr int ESP_GAP_BLE_SCAN_RESULT_EVT=58;
constexpr int ESP_GAP_BLE_SCAN_START_COMPLETE_EVT=59;
constexpr int ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT=60;
constexpr int ESP_GAP_SEARCH_INQ_CMPL_EVT=61;
constexpr int ESP_GAP_SEARCH_INQ_RES_EVT=62;
constexpr int ESP_GATTC_CFG_MTU_EVT=63;
constexpr int ESP_GATTC_DISCONNECT_EVT=64;
constexpr int ESP_GATTC_DIS_SRVC_CMPL_EVT=65;
constexpr int ESP_GATTC_EXEC_EVT=66;
constexpr int ESP_GATTC_NOTIFY_EVT=67;
constexpr int ESP_GATTC_OPEN_EVT=68;
constexpr int ESP_GATTC_PREP_WRITE_EVT=69;
constexpr int ESP_GATTC_READ_CHAR_EVT=70;
constexpr int ESP_GATTC_REG_EVT=71;
constexpr int ESP_GATTC_REG_FOR_NOTIFY_EVT=72;
constexpr int ESP_GATTC_SEARCH_CMPL_EVT=73;
constexpr int ESP_GATTC_SEARCH_RES_EVT=74;
constexpr int ESP_GATTC_SRVC_CHG_EVT=75;
constexpr int ESP_GATTC_WRITE_CHAR_EVT=76;
constexpr int ESP_GATTC_WRITE_DESCR_EVT=77;
constexpr int ESP_GATT_AUTH_REQ_NONE=78;
constexpr int ESP_GATT_CHAR_PROP_BIT_INDICATE=79;
constexpr int ESP_GATT_CHAR_PROP_BIT_READ=80;
constexpr int ESP_GATT_CHAR_PROP_BIT_WRITE=81;
constexpr int ESP_GATT_IF_NONE=82;
constexpr int ESP_GATT_UUID_CHAR_CLIENT_CONFIG=84;
constexpr int ESP_GATT_WRITE_TYPE_NO_RSP=85;
constexpr int ESP_GATT_WRITE_TYPE_RSP=86;
constexpr int ESP_UUID_LEN_128=91;
constexpr int ESP_UUID_LEN_16=92;
constexpr int MALLOC_CAP_8BIT=93;
constexpr int MALLOC_CAP_SPIRAM=94;
constexpr int NVS_READONLY=95;
constexpr int NVS_READWRITE=96;

using esp_bd_addr_t=uint8_t[6];
using esp_bt_pin_code_t=uint8_t[16];
using esp_gatt_if_t=int;
using esp_ble_addr_type_t=int;
using esp_gap_ble_cb_event_t=int;
using esp_gattc_cb_event_t=int;
using esp_bt_gap_cb_event_t=int;
using esp_a2d_cb_event_t=int;
struct esp_bt_uuid_t { int len; union { uint16_t uuid16; uint8_t uuid128[16]; } uuid; };
struct esp_a2d_cie_sbc_t { uint8_t ch_mode:4,samp_freq:4; uint8_t alloc_mthd:2,num_subbands:2,block_len:4; uint8_t min_bitpool,max_bitpool; };
struct esp_a2d_mcc_t { int type; struct { esp_a2d_cie_sbc_t sbc_info; } cie; };
struct esp_a2d_cb_param_t {
 struct { int state; esp_bd_addr_t remote_bda; } conn_stat;
 struct { int state; } audio_stat;
 struct { int cmd,status; } media_ctrl_stat;
 struct { esp_a2d_mcc_t mcc; } audio_cfg;
};
struct esp_ble_scan_params_t { int scan_type,own_addr_type,scan_filter_policy,scan_interval,scan_window,scan_duplicate; };
struct esp_ble_gap_cb_param_t {
 struct { int status; } scan_param_cmpl,scan_start_cmpl,scan_stop_cmpl;
 struct { int search_evt; esp_bd_addr_t bda; int ble_addr_type; uint8_t ble_adv[64]; int scan_rsp_len,adv_data_len; } scan_rst;
};
using esp_gap_ble_cb_t=void(*)(esp_gap_ble_cb_event_t,esp_ble_gap_cb_param_t*);
struct esp_ble_gattc_cb_param_t {
 struct { int app_id,status; } reg;
 struct { int status,conn_id,mtu; esp_bd_addr_t remote_bda; } open;
 struct { int status,conn_id,mtu; } cfg_mtu;
 struct { int status,conn_id; } dis_srvc_cmpl;
 struct { int conn_id,start_handle,end_handle; struct { esp_bt_uuid_t uuid; } srvc_id; } search_res;
 struct { int status,conn_id; } search_cmpl;
 struct { int status,handle; } reg_for_notify;
 struct { int status,conn_id,handle,offset; } write;
 struct { int status,conn_id; } exec_cmpl;
 struct { int conn_id,handle,is_notify; } notify;
 struct { int status,conn_id,handle; uint16_t value_len; uint8_t* value; } read;
 struct { int conn_id,reason; esp_bd_addr_t remote_bda; } disconnect;
 struct { esp_bd_addr_t remote_bda; } srvc_chg;
};
struct esp_gattc_descr_elem_t { uint16_t handle; };
struct esp_gattc_char_elem_t { uint16_t char_handle; int properties; };
struct esp_bt_gap_dev_prop_t { int type,len; void* val; };
struct esp_bt_gap_cb_param_t {
 struct { esp_bd_addr_t bda; int num_prop; esp_bt_gap_dev_prop_t* prop; } disc_res;
 struct { int state; } disc_st_chg;
 struct { esp_bd_addr_t bda; int min_16_digit; } pin_req;
 struct { esp_bd_addr_t bda; int num_val; } cfm_req;
 struct { esp_bd_addr_t bda; int stat; } auth_cmpl;
 struct { esp_bd_addr_t bda; int stat,handle; } acl_conn_cmpl_stat;
 struct { esp_bd_addr_t bda; int reason,handle; } acl_disconn_cmpl_stat;
};
struct esp_bt_controller_config_t {};
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() esp_bt_controller_config_t{}
namespace radio_mock {
inline int64_t time=1;
inline unsigned connects=0,disconnects=0,inquiries=0,media_requests=0;
inline esp_err_t connect_result=ESP_OK,inquiry_result=ESP_OK;
inline std::function<int(void*)> receive;
inline void log(const char*,const char*,...) {}
}
#define ESP_LOGI(...) radio_mock::log(__VA_ARGS__)
#define ESP_LOGW(...) radio_mock::log(__VA_ARGS__)
#define ESP_LOGE(...) radio_mock::log(__VA_ARGS__)
inline const char* esp_err_to_name(int) { return "fake"; }
inline int64_t esp_timer_get_time() { return radio_mock::time; }
inline esp_err_t esp_a2d_source_connect(const uint8_t*) { ++radio_mock::connects; return radio_mock::connect_result; }
inline esp_err_t esp_a2d_source_disconnect(const uint8_t*) { ++radio_mock::disconnects; return ESP_OK; }
inline esp_err_t esp_bt_gap_start_discovery(int,int,int) { ++radio_mock::inquiries; return radio_mock::inquiry_result; }
inline esp_err_t esp_a2d_media_ctrl(int) { ++radio_mock::media_requests; return ESP_OK; }
inline uint8_t* esp_ble_resolve_adv_data(uint8_t*,int,uint8_t*) { return nullptr; }
inline uint8_t* esp_bt_gap_resolve_eir_data(uint8_t*,int,uint8_t*) { return nullptr; }
inline esp_gap_ble_cb_t esp_ble_gap_get_callback() { return nullptr; }
using nvs_handle_t=int;
constexpr int pdTRUE=1,pdPASS=1;
using QueueHandle_t=void*;
struct StaticQueue_t {};
using TaskHandle_t=void*;
#define pdMS_TO_TICKS(value) (value)
inline int xQueueSend(QueueHandle_t,const void*,int) { return pdTRUE; }
inline int xQueueReceive(QueueHandle_t,void* event,int) { return radio_mock::receive(event); }
inline QueueHandle_t xQueueCreateStatic(int,int,uint8_t*,StaticQueue_t*) { return reinterpret_cast<void*>(1); }
inline int xTaskCreate(void(*)(void*),const char*,int,void*,int,TaskHandle_t*) { return pdPASS; }
inline unsigned uxTaskGetStackHighWaterMark(TaskHandle_t) { return 0; }
inline void* heap_caps_malloc(size_t size,int) { return std::malloc(size); }
inline void heap_caps_free(void* p) { std::free(p); }
inline int nvs_open(const char*,int,nvs_handle_t*) { return ESP_ERR_NOT_FOUND; }
inline int nvs_get_blob(nvs_handle_t,const char*,void*,size_t*) { return ESP_ERR_NOT_FOUND; }
inline void nvs_close(nvs_handle_t) {}
inline int nvs_set_blob(nvs_handle_t,const char*,const void*,size_t) { return ESP_OK; }
inline int nvs_commit(nvs_handle_t) { return ESP_OK; }
template<class... Args> inline int esp_a2d_register_callback(Args...) { return ESP_OK; }
template<class... Args> inline int esp_a2d_source_init(Args...) { return ESP_OK; }
template<class... Args> inline int esp_a2d_source_register_data_callback(Args...) { return ESP_OK; }
template<class... Args> inline int esp_a2d_source_register_stream_endpoint(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gap_register_callback(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gap_set_scan_params(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gap_start_scanning(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gap_stop_scanning(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gatt_set_local_mtu(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_app_register(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_close(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_execute_write(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_get_char_by_uuid(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_get_descr_by_char_handle(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_open(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_prepare_write(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_read_char(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_register_callback(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_register_for_notify(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_search_service(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_send_mtu_req(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_write_char(Args...) { return ESP_OK; }
template<class... Args> inline int esp_ble_gattc_write_char_descr(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bluedroid_enable(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bluedroid_init(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_controller_enable(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_controller_init(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_dev_set_device_name(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_gap_cancel_discovery(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_gap_pin_reply(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_gap_register_callback(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_gap_set_scan_mode(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_gap_set_security_param(Args...) { return ESP_OK; }
template<class... Args> inline int esp_bt_gap_ssp_confirm_reply(Args...) { return ESP_OK; }
