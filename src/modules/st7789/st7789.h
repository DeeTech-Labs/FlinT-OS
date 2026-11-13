#pragma once

/**
 * @brief ST7789/ST7789V2 по SPI2: инициализация панели и синхронная передача RGB565.
 * Включение: [modules.st7789] и вложенная panel в device.toml. GPIO, команды панели,
 * GRAM и ориентация задаются конфигом; значения по умолчанию другой платы не применять.
 * Порядок: begin(), setWindow(x,y,w,h), writePixels(wireOrderPixels), displayOn().
 * Пример после begin: setWindow(0,0,1,1); writePixels(onePixel); ошибки проверять по ESP_OK.
 * Pixels должны быть в wire-order RGB565 и в DMA-доступной памяти, до MaxTransferPixels.
 * Каждое окно нужно заполнить полностью; displayOn вызывается после первого заполнения.
 * Трансфер завершается до возврата, буфер можно переиспользовать сразу. Только Core,
 * без ISR/параллельного доступа. Драйвер владеет SPI device и созданной им SPI2-шиной.
 * В OS используйте display/display.h: адаптер управляет framebuffer, dirty regions и recovery.
 */

#include "board/device_config.h"
#include "driver/spi_master.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace flint::modules {

// Owned by the application task. All transfers finish before returning; callers may
// reuse their DMA-capable, wire-order RGB565 buffer immediately after writePixels.
class St7789 {
public:
    static constexpr size_t TransferRows = 2;
    static constexpr size_t MaxTransferPixels = FLINT_FB_MAX_W * TransferRows;
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

} // namespace flint::modules
