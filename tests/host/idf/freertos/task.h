#pragma once
#include "FreeRTOS.h"
void vTaskDelay(TickType_t);
using TaskHandle_t = void *;
using BaseType_t = int;
inline constexpr int pdFALSE = 0, pdTRUE = 1, eSetBits = 1;
TaskHandle_t xTaskGetCurrentTaskHandle();
BaseType_t xTaskNotifyFromISR(TaskHandle_t, uint32_t, int, BaseType_t *);
#define portYIELD_FROM_ISR() ((void)0)
