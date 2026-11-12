#include "st7789.h"
#include "board/device_config.h"
#include "board/device_display.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cstring>

namespace flint::display {
namespace {
constexpr const char *Tag = "st7789";

void delayMs(unsigned milliseconds) {
    vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(milliseconds)));
}

void logError(esp_err_t error) {
    if (error != ESP_OK)
        ESP_LOGE(Tag, "%s", esp_err_to_name(error));
}
} // namespace

St7789::~St7789() {
    release();
}

void St7789::release() {
    ready_ = false;
    remainingPixels_ = 0;
    if (device_) {
        logError(spi_bus_remove_device(device_));
        device_ = nullptr;
    }
    if (busOwned_) {
        logError(spi_bus_free(SPI2_HOST));
        busOwned_ = false;
    }
}

esp_err_t St7789::transfer(bool data, const void *bytes, size_t size) {
    if (!device_)
        return ESP_ERR_INVALID_STATE;
    auto error = gpio_set_level(gpio_num_t(FLINT_TFT_DC), data ? 1 : 0);
    if (error != ESP_OK)
        return error;
    spi_transaction_t transaction{};
    transaction.length = size * 8;
    if (size <= sizeof(transaction.tx_data)) {
        transaction.flags = SPI_TRANS_USE_TXDATA;
        std::memcpy(transaction.tx_data, bytes, size);
    } else {
        transaction.tx_buffer = bytes;
    }
    // DMA + interrupt completion, without polling or retaining the caller's buffer.
    return spi_device_transmit(device_, &transaction);
}

esp_err_t St7789::command(uint8_t value, std::span<const uint8_t> parameters) {
    auto error = transfer(false, &value, 1);
    if (error == ESP_OK && !parameters.empty())
        error = transfer(true, parameters.data(), parameters.size());
    return error;
}

esp_err_t St7789::begin() {
    if (ready_)
        return ESP_OK;
    const auto fail = [this](esp_err_t error) {
        release();
        return error;
    };
    gpio_config_t pins{};
    pins.pin_bit_mask = (1ULL << FLINT_TFT_DC) | (1ULL << FLINT_TFT_RST);
    pins.mode = GPIO_MODE_OUTPUT;
    auto error = gpio_config(&pins);
    if (error != ESP_OK)
        return error;
    error = gpio_set_level(gpio_num_t(FLINT_TFT_RST), 0);
    if (error != ESP_OK)
        return error;
    delayMs(8);

    spi_bus_config_t bus{};
    bus.mosi_io_num = FLINT_TFT_MOSI;
    bus.miso_io_num = FLINT_TFT_MISO;
    bus.sclk_io_num = FLINT_TFT_SCLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = MaxTransferPixels * sizeof(uint16_t);
    error = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (error != ESP_OK)
        return error;
    busOwned_ = true;

    spi_device_interface_config_t config{};
    config.clock_speed_hz = FLINT_SPI_FREQ_WRITE;
    config.mode = 0;
    config.spics_io_num = FLINT_TFT_CS;
    config.queue_size = 1;
    config.flags = SPI_DEVICE_HALFDUPLEX | (FLINT_SPI_3WIRE ? SPI_DEVICE_3WIRE : 0);
    error = spi_bus_add_device(SPI2_HOST, &config, &device_);
    if (error != ESP_OK)
        return fail(error);
    error = gpio_set_level(gpio_num_t(FLINT_TFT_RST), 1);
    if (error != ESP_OK)
        return fail(error);
    delayMs(64);

    for (const auto &entry : board::St7789Init) {
        error = command(entry.command, {entry.data.data(), entry.size});
        if (error != ESP_OK)
            return fail(error);
        if (entry.delayMs)
            delayMs(entry.delayMs);
    }
    error = command(FLINT_PANEL_INVERT ? 0x21 : 0x20); // INVON / INVOFF
    const uint8_t colorMode = 0x55;                    // RGB565
    if (error == ESP_OK)
        error = command(0x3a, {&colorMode, 1}); // COLMOD
    const uint8_t orientation = board::St7789Madctl;
    if (error == ESP_OK)
        error = command(0x36, {&orientation, 1}); // MADCTL
    if (error != ESP_OK)
        return fail(error);
    ready_ = true;
    return ESP_OK;
}

esp_err_t St7789::setWindow(int x, int y, int width, int height) {
    remainingPixels_ = 0;
    if (!ready_)
        return ESP_ERR_INVALID_STATE;
    if (x < 0 || y < 0 || width <= 0 || height <= 0 || x >= FLINT_DESIGN_W || y >= FLINT_DESIGN_H ||
        width > FLINT_DESIGN_W - x || height > FLINT_DESIGN_H - y)
        return ESP_ERR_INVALID_ARG;
    x += board::St7789ColumnOffset;
    y += board::St7789RowOffset;
    const int endX = x + width - 1, endY = y + height - 1;
    const uint8_t columns[]{uint8_t(x >> 8), uint8_t(x), uint8_t(endX >> 8), uint8_t(endX)};
    const uint8_t rows[]{uint8_t(y >> 8), uint8_t(y), uint8_t(endY >> 8), uint8_t(endY)};
    auto error = command(0x2a, columns); // CASET, inclusive endpoints
    if (error == ESP_OK)
        error = command(0x2b, rows); // RASET
    if (error == ESP_OK)
        error = command(0x2c); // RAMWR; subsequent data transfers continue the window
    if (error == ESP_OK)
        remainingPixels_ = size_t(width) * height;
    return error;
}

esp_err_t St7789::writePixels(std::span<const uint16_t> pixels) {
    if (!ready_ || !remainingPixels_)
        return ESP_ERR_INVALID_STATE;
    if (pixels.empty() || pixels.size() > remainingPixels_ || pixels.size() > MaxTransferPixels)
        return ESP_ERR_INVALID_ARG;
    const auto error = transfer(true, pixels.data(), pixels.size_bytes());
    if (error == ESP_OK)
        remainingPixels_ -= pixels.size();
    else
        remainingPixels_ = 0;
    return error;
}

esp_err_t St7789::displayOn() {
    if (!ready_ || remainingPixels_)
        return ESP_ERR_INVALID_STATE;
    return command(0x29); // DISPON only after the initial background is written
}

} // namespace flint::display
