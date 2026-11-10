#pragma once
#include <chrono>
#include <cstdint>
#ifdef FLINT_USB_TEST_CLOCK
extern int64_t usbTestClock;
inline int64_t esp_timer_get_time() { return usbTestClock; }
#else
inline int64_t esp_timer_get_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif
