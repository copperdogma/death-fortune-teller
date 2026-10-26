#pragma once
#include "FreeRTOS.h"
#include <functional>
using TaskHandle_t = std::thread *;
namespace printer_stub {
inline std::mutex delay_hook_mutex;
inline std::function<void(TickType_t)> delay_hook;
inline void set_delay_hook(std::function<void(TickType_t)> hook) {
    std::lock_guard<std::mutex> lock(delay_hook_mutex); delay_hook = std::move(hook);
}
}
inline void vTaskDelay(TickType_t ticks) {
    std::function<void(TickType_t)> hook;
    { std::lock_guard<std::mutex> lock(printer_stub::delay_hook_mutex); hook = printer_stub::delay_hook; }
    if (hook) hook(ticks);
    else std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}
inline BaseType_t xTaskCreate(void (*entry)(void *), const char *, uint32_t, void *arg, unsigned, TaskHandle_t *handle) {
    auto *thread = new std::thread([=]{ entry(arg); }); thread->detach(); if (handle) *handle = thread; return pdPASS;
}
