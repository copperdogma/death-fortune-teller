#pragma once
#include <mutex>
#include <deque>
#include <cassert>
struct StaticQueue_t { std::mutex mutex; std::deque<uint8_t> data; unsigned capacity=0; bool producer_ready=false; };
using QueueHandle_t=StaticQueue_t *;
inline QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,uint8_t*,StaticQueue_t *q){assert(size==1);q->capacity=n;static unsigned created=0;q->producer_ready=(++created==2);return q;}
inline void xQueueReset(QueueHandle_t q){std::lock_guard<std::mutex> lock(q->mutex);q->data.clear();}
inline int xQueueSend(QueueHandle_t q,const uint8_t *v,unsigned){std::lock_guard<std::mutex> lock(q->mutex);if(q->data.size()>=q->capacity)return 0;if(q->producer_ready){unsigned burst=++audio_test_publications_since_delay;assert(burst<=2 && "producer must block after at most two publications");unsigned old=audio_test_max_burst.load();while(old<burst&&!audio_test_max_burst.compare_exchange_weak(old,burst)){} }q->data.push_back(*v);return 1;}
inline int xQueueReceive(QueueHandle_t q,uint8_t *v,unsigned){std::lock_guard<std::mutex> lock(q->mutex);if(q->data.empty())return 0;*v=q->data.front();q->data.pop_front();return 1;}
inline unsigned uxQueueMessagesWaiting(QueueHandle_t q){std::lock_guard<std::mutex> lock(q->mutex);return q->data.size();}
