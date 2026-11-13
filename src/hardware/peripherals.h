#pragma once
#include "esp_err.h"
#include <ctime>
#include <flint/core.h>
#include <flint/sensors.h>
// Core-side nonblocking adapters. Only the hardware worker accesses I2C/ADC.
void deviceHardwareBegin();
flint::sensors::Backend *deviceSensorBackend();
void deviceHardwareWakeFromIsr();
void deviceHardwareDiagnostics(flint::Values &values);
flint::Error devicePowerStatus(std::string_view key, flint::AppReply &reply);
esp_err_t deviceBacklight(unsigned percent);
esp_err_t deviceTone(unsigned frequency, unsigned durationMs);
esp_err_t deviceSetMuted(bool muted);
flint::Error deviceRtcReadEpoch(uint64_t &epoch);
flint::Error deviceRtcRequest(uint64_t epoch, uint64_t &token);
flint::Error deviceRtcResult(uint64_t token, bool &complete);
void deviceHardwareMaintain();
void deviceButtonsInterrupt();
uint64_t deviceHardwareNextWake();
// Call only after durable storage and transports have been quiesced.
esp_err_t deviceReleasePower();
