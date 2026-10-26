#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <mutex>
using esp_err_t=int;
constexpr esp_err_t ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NVS_NOT_ENOUGH_SPACE=100;
const char *esp_err_to_name(esp_err_t);
using vprintf_like_t=int(*)(const char *,va_list);
vprintf_like_t esp_log_set_vprintf(vprintf_like_t);
void test_log(const char *,const char *,const char *,...);
#define ESP_LOGI(tag,...) test_log("I",tag,__VA_ARGS__)
#define ESP_LOGW(tag,...) test_log("W",tag,__VA_ARGS__)
#define ESP_LOGE(tag,...) test_log("E",tag,__VA_ARGS__)
using portMUX_TYPE=std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(p) (p)->lock()
#define portEXIT_CRITICAL(p) (p)->unlock()
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
void *heap_caps_malloc(size_t,int);
int64_t esp_timer_get_time();
uint32_t esp_random();
struct esp_app_desc_t {uint8_t app_elf_sha256[32];};
const esp_app_desc_t *esp_app_get_description();
using nvs_handle_t=unsigned;
constexpr int NVS_READWRITE=1,NVS_READONLY=0;
struct nvs_stats_t{size_t available_entries;};
esp_err_t nvs_get_stats(const char *,nvs_stats_t *);
esp_err_t nvs_open(const char *,int,nvs_handle_t *);
esp_err_t nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
esp_err_t nvs_set_u64(nvs_handle_t,const char *,uint64_t);
esp_err_t nvs_get_u64(nvs_handle_t,const char *,uint64_t *);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
