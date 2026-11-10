#pragma once

#include "driver/spi_master.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace flint::display {

// Owned by the application task. All transfers finish before returning; callers may
// reuse their DMA-capable, wire-order RGB565 buffer immediately after writePixels.
class St7789 {
public:
    St7789() = default;
    ~St7789();
    St7789(const St7789 &) = delete;
    St7789 &operator=(const St7789 &) = delete;

    esp_err_t begin();
    esp_err_t setWindow(int x, int y, int width, int height);
    esp_err_t writePixels(std::span<const uint16_t> pixels);
    esp_err_t displayOn();
    bool ready() const {
        return ready_;
    }

private:
    spi_device_handle_t device_{};
    bool busOwned_{};
    bool ready_{};
    size_t remainingPixels_{};

    esp_err_t transfer(bool data, const void *bytes, size_t size);
    esp_err_t command(uint8_t value, std::span<const uint8_t> parameters = {});
    void release();
};

} // namespace flint::display
