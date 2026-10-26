#pragma once
#include "FreeRTOS.h"
#include <condition_variable>
#include <cstring>
#include <functional>
#include <deque>
#include <mutex>
#include <vector>
struct FakeQueue {
    size_t item_size, capacity;
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<std::vector<char>> items;
};
using QueueHandle_t = FakeQueue *;
namespace printer_stub {
inline std::mutex hook_mutex;
inline std::function<void()> receive_hook;
inline void set_receive_hook(std::function<void()> hook) {
    std::lock_guard<std::mutex> lock(hook_mutex); receive_hook = std::move(hook);
}
inline void run_receive_hook() {
    std::function<void()> hook;
    { std::lock_guard<std::mutex> lock(hook_mutex); hook = receive_hook; }
    if (hook) hook();
}
}
inline QueueHandle_t xQueueCreate(size_t capacity, size_t item_size) {
    auto *q = new FakeQueue; q->item_size = item_size; q->capacity = capacity; return q;
}
inline BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t) {
    std::lock_guard<std::mutex> lock(q->mutex);
    if (q->items.size() == q->capacity) return 0;
    std::vector<char> copy(q->item_size); std::memcpy(copy.data(), item, q->item_size);
    q->items.push_back(std::move(copy)); q->ready.notify_one(); return pdTRUE;
}
inline BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait) {
    std::unique_lock<std::mutex> lock(q->mutex);
    if (wait == portMAX_DELAY) q->ready.wait(lock, [&]{ return !q->items.empty(); });
    else if (q->items.empty()) return 0;
    std::memcpy(item, q->items.front().data(), q->item_size); q->items.pop_front();
    lock.unlock(); printer_stub::run_receive_hook(); return pdTRUE;
}
inline size_t uxQueueMessagesWaiting(QueueHandle_t q) {
    std::lock_guard<std::mutex> lock(q->mutex); return q->items.size();
}
inline void vQueueDelete(QueueHandle_t q) { delete q; }
