#pragma once
#include <thread>
#include <chrono>
#include <cassert>
using TaskHandle_t = void *;
inline int xTaskCreate(void (*f)(void*), const char*, unsigned, void *p, unsigned, TaskHandle_t *h){*h=reinterpret_cast<void*>(1); std::thread(f,p).detach();return 1;}
inline void vTaskDelay(unsigned n){audio_test_publications_since_delay=0;assert(n > 0 && "audio worker must block at 100 Hz tick rate");std::this_thread::sleep_for(std::chrono::milliseconds(n * 10));}
inline unsigned uxTaskGetStackHighWaterMark(void*){return 1234;}
