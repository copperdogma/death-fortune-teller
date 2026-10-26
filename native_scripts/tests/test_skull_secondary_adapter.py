#!/usr/bin/env python3
"""Exercise actual secondary.cpp against bounded host Bluetooth/queue shims."""
import pathlib
import re
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
STUB = r'''
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>
using esp_err_t=int; using esp_gatt_if_t=int; using esp_gatt_status_t=int;
using esp_gatts_cb_event_t=int; using esp_gap_ble_cb_event_t=int;
CONSTANTS
constexpr int MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2, ADV_TYPE_IND=0, BLE_ADDR_TYPE_PUBLIC=0,
 ADV_CHNL_ALL=7, ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY=0, pdTRUE=1;
struct esp_gatt_rsp_t { struct {uint16_t handle,offset,len;uint8_t value[600];} attr_value; };
struct esp_ble_adv_params_t {int adv_int_min,adv_int_max,adv_type,own_addr_type,channel_map,adv_filter_policy;};
struct esp_gatts_attr_db_t {struct{int auto_rsp;} control;struct{int uuid_length;uint8_t*uuid;int perm,max_length,length;uint8_t*value;} att_desc;};
struct esp_ble_gatts_cb_param_t {
 struct {int app_id,status;} reg;
 struct {int status;uint16_t num_handle;uint16_t* handles;} add_attr_tab;
 struct {int status;} start;
 struct {uint16_t conn_id;} connect,disconnect;
 struct {uint16_t conn_id,mtu;} mtu;
 struct {uint16_t conn_id;uint32_t trans_id;uint16_t handle,offset;bool need_rsp;} read;
 struct {uint16_t conn_id;uint32_t trans_id;uint16_t handle,offset;bool need_rsp,is_prep;uint16_t len;uint8_t*value;} write;
 struct {uint16_t conn_id;uint32_t trans_id;int exec_write_flag;} exec_write;
};
struct esp_ble_gap_cb_param_t {struct {int status;} adv_data_raw_cmpl,scan_rsp_data_raw_cmpl,adv_start_cmpl,adv_stop_cmpl;};
using esp_gatts_cb_t=void(*)(int,int,esp_ble_gatts_cb_param_t*);
inline esp_gatts_cb_t registered_callback=nullptr;
inline int responses=0, notifications=0;
inline int64_t fake_now=1;
struct StaticQueue_t {size_t capacity,size;std::deque<std::vector<uint8_t>> values;};
using QueueHandle_t=StaticQueue_t*;
inline QueueHandle_t xQueueCreateStatic(int capacity,size_t size,uint8_t*,StaticQueue_t* q){q->capacity=capacity;q->size=size;return q;}
inline int xQueueSend(QueueHandle_t q,const void* data,int){if(q->values.size()==q->capacity)return 0;q->values.emplace_back((const uint8_t*)data,(const uint8_t*)data+q->size);return 1;}
inline int xQueueReceive(QueueHandle_t q,void* data,int){if(q->values.empty())return 0;std::memcpy(data,q->values.front().data(),q->size);q->values.pop_front();return 1;}
inline void* heap_caps_malloc(size_t size,int){return std::malloc(size);} inline void heap_caps_free(void*p){std::free(p);}
inline int64_t esp_timer_get_time(){return fake_now;}
inline esp_gatts_cb_t esp_ble_gatts_get_callback(){return registered_callback;}
inline int esp_ble_gatts_register_callback(esp_gatts_cb_t cb){registered_callback=cb;return ESP_OK;}
inline int esp_ble_gatts_app_register(int){return ESP_OK;}
inline int esp_ble_gatts_create_attr_tab(const esp_gatts_attr_db_t*,int,int,int){return ESP_OK;}
inline int esp_ble_gatts_start_service(int){return ESP_OK;}
inline int esp_ble_gatts_close(int,int){return ESP_OK;}
inline int esp_ble_gatts_send_response(int,int,uint32_t,int,esp_gatt_rsp_t*){++responses;return ESP_OK;}
inline int esp_ble_gatts_send_indicate(int,int,int,size_t,uint8_t*,bool){++notifications;return ESP_OK;}
inline int esp_ble_gap_start_advertising(esp_ble_adv_params_t*){return ESP_OK;}
inline int esp_ble_gap_stop_advertising(){return ESP_OK;}
inline int esp_ble_gap_config_adv_data_raw(uint8_t*,size_t){return ESP_OK;}
inline int esp_ble_gap_config_scan_rsp_data_raw(uint8_t*,size_t){return ESP_OK;}
'''
TEST = r'''
#include <cassert>
#include "skull_secondary.cpp"
using namespace skull::secondary;
static void dispatch(int kind,esp_ble_gatts_cb_param_t &p){registered_callback(kind,1,&p);}
static void connect(){esp_ble_gatts_cb_param_t p{};p.connect.conn_id=7;dispatch(ESP_GATTS_CONNECT_EVT,p);poll();}
static void disconnect(){esp_ble_gatts_cb_param_t p{};p.disconnect.conn_id=7;dispatch(ESP_GATTS_DISCONNECT_EVT,p);}
static void subscribe(){esp_ble_gatts_cb_param_t p{};uint8_t c[]={1,0};p.write={7,1,13,0,true,false,2,c};dispatch(ESP_GATTS_WRITE_EVT,p);poll();}
int main(){
 assert(init()==ESP_OK);
 esp_ble_gatts_cb_param_t p{};p.reg={0x534c,ESP_GATT_OK};dispatch(ESP_GATTS_REG_EVT,p);poll();
 uint16_t handles[]={10,11,12,13};p={};p.add_attr_tab={ESP_GATT_OK,4,handles};dispatch(ESP_GATTS_CREAT_ATTR_TAB_EVT,p);poll();
 p={};p.start.status=ESP_GATT_OK;dispatch(ESP_GATTS_START_EVT,p);poll();
 connect();subscribe();busy(false);
 // All four queued ATT events must be discarded after callback observes
 // disconnect, even though poll has not yet consumed that disconnect.
 uint8_t path[]="/audio/Test.wav";
 p={};p.read={7,2,12,0,true};dispatch(ESP_GATTS_READ_EVT,p);
 p={};p.write={7,3,12,0,true,false,sizeof(path)-1,path};dispatch(ESP_GATTS_WRITE_EVT,p);
 p={};p.exec_write={7,4,ESP_GATT_PREP_WRITE_EXEC};dispatch(ESP_GATTS_EXEC_WRITE_EVT,p);
 p={};p.mtu={7,517};dispatch(ESP_GATTS_MTU_EVT,p);
 disconnect();
 // Reuse the same connection ID before draining any of the old transactions.
 p={};p.connect.conn_id=7;dispatch(ESP_GATTS_CONNECT_EVT,p);
 const int before=responses;poll();assert(responses==before);
 Request r{};assert(!take_request(r));assert(connected());
 // A new-generation write still works, proving the stale guard did not simply
 // disable all requests after reconnection.
 subscribe();busy(false);
 p={};p.write={7,5,12,0,true,false,sizeof(path)-1,path};dispatch(ESP_GATTS_WRITE_EVT,p);poll();
 assert(take_request(r));assert(std::strcmp(r.path,(const char*)path)==0);
 auto stale=r.token;disconnect();assert(!respond(stale,true));poll();
 assert(notifications==0);
 // A later request gets a distinct token and can be accepted exactly once.
 connect();subscribe();busy(false);
 p={};p.write={7,6,12,0,true,false,sizeof(path)-1,path};dispatch(ESP_GATTS_WRITE_EVT,p);poll();
 assert(take_request(r));assert(r.token!=stale);assert(!respond(stale,true));
 assert(respond(r.token,true));assert(notifications==1);assert(!respond(r.token,true));
}
'''

class AdapterTest(unittest.TestCase):
    def test_queued_att_disconnect_and_reused_connection(self):
        source = (ROOT / 'firmware/main/skull_secondary.cpp').read_text()
        identifiers = sorted(set(re.findall(r'\bESP_[A-Z0-9_]+', source)))
        values = {'ESP_OK': 0, 'ESP_GATT_OK': 0, 'ESP_BT_STATUS_SUCCESS': 0,
                  'ESP_GATT_IF_NONE': 255, 'ESP_GATT_PREP_WRITE_CANCEL': 0,
                  'ESP_GATT_PREP_WRITE_EXEC': 1}
        constants = '\n'.join(f'constexpr int {name}={values.get(name, index+100)};'
                              for index, name in enumerate(identifiers))
        with tempfile.TemporaryDirectory() as temporary:
            directory = pathlib.Path(temporary)
            (directory / 'stub.h').write_text(STUB.replace('CONSTANTS', constants))
            for name in ['esp_err.h','esp_gap_ble_api.h','esp_gatts_api.h','esp_gatt_common_api.h',
                         'esp_heap_caps.h','esp_timer.h','freertos/FreeRTOS.h','freertos/queue.h']:
                path = directory / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('#include "stub.h"\n')
            (directory / 'test.cpp').write_text(TEST)
            executable = directory / 'test'
            subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',f'-I{directory}',
                            f'-I{ROOT / "firmware/main"}',str(directory/'test.cpp'),'-o',str(executable)],check=True)
            subprocess.run([str(executable)],check=True)

if __name__ == '__main__':
    unittest.main()
