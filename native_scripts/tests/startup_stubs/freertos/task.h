#pragma once
#ifndef INC_FREERTOS_H
#error "Include freertos/FreeRTOS.h before freertos/task.h, as required by ESP-IDF"
#endif
inline void vTaskDelay(unsigned) {}
