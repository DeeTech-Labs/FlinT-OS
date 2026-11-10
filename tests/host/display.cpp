#include "display/display.h"
#include "board/device_config.h"
#include "display/st7789.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include <array>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition))                                                                          \
            throw std::runtime_error(#condition);                                                  \
    } while (false)

namespace {
struct Transfer {
    bool data;
    std::vector<uint8_t> bytes;
};
struct Fake {
    bool bus{}, dc{}, failAllocation{}, noPsram{}, failBus{}, failDevice{};
    int failTransfer{}, attempts{}, allocations{}, frees{}, busFrees{}, deviceRemovals{};
    int command{}, x0{}, x1{}, y0{}, y1{}, x{}, y{};
    size_t pixelBytes{};
    spi_bus_config_t busConfig{};
    spi_device_interface_config_t deviceConfig{};
    std::vector<Transfer> transfers;
    std::vector<unsigned> delays;
    std::array<uint16_t, 280 * 240> gram{};
};
Fake &fake() {
    // Outlive firmware singletons whose destructors release the simulated bus.
    static auto *state = new Fake;
    return *state;
}
void clearTransfers() {
    auto &s = fake();
    s.transfers.clear();
    s.attempts = 0;
    s.pixelBytes = 0;
}
const Transfer &commandData(uint8_t command) {
    const auto &transfers = fake().transfers;
    for (size_t i = 0; i + 1 < transfers.size(); ++i)
        if (!transfers[i].data && transfers[i].bytes == std::vector<uint8_t>{command})
            return transfers[i + 1];
    throw std::runtime_error("command not found");
}
bool hasCommand(uint8_t command) {
    for (const auto &transfer : fake().transfers)
        if (!transfer.data && transfer.bytes == std::vector<uint8_t>{command})
            return true;
    return false;
}
void driverTests() {
    auto &s = fake();
    {
        flint::display::St7789 panel;
        CHECK(panel.setWindow(0, 0, 1, 1) == ESP_ERR_INVALID_STATE);
        CHECK(panel.begin() == ESP_OK);
        CHECK(s.busConfig.mosi_io_num == 11 && s.busConfig.sclk_io_num == 12);
        CHECK(s.busConfig.miso_io_num == -1 && s.busConfig.max_transfer_sz == 560);
        CHECK(s.deviceConfig.spics_io_num == 10 && s.deviceConfig.clock_speed_hz == 80000000);
        CHECK(s.deviceConfig.flags == (SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE));
        CHECK(s.delays == (std::vector<unsigned>{8, 64, 130}));
        CHECK(commandData(0xb0).bytes == (std::vector<uint8_t>{0x00, 0xc0}));
        CHECK(commandData(0xe0).bytes ==
            (std::vector<uint8_t>{0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0e,
                0x12, 0x14, 0x17}));
        CHECK(commandData(0xe1).bytes ==
            (std::vector<uint8_t>{0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x31, 0x54, 0x47, 0x0e, 0x1c,
                0x17, 0x1b, 0x1e}));
        CHECK(commandData(0x36).bytes == std::vector<uint8_t>{0xb8});
        CHECK(commandData(0x3a).bytes == std::vector<uint8_t>{0x55});
        CHECK(hasCommand(0x21) && !hasCommand(0x29));
        const auto attempts = s.attempts;
        CHECK(panel.begin() == ESP_OK && s.attempts == attempts);
        clearTransfers();
        CHECK(panel.setWindow(0, 0, 280, 240) == ESP_OK);
        CHECK(commandData(0x2a).bytes == (std::vector<uint8_t>{0, 20, 1, 43}));
        CHECK(commandData(0x2b).bytes == (std::vector<uint8_t>{0, 0, 0, 239}));
        CHECK(panel.displayOn() == ESP_ERR_INVALID_STATE);
        CHECK(panel.setWindow(279, 239, 1, 1) == ESP_OK);
        alignas(4) const uint16_t pixel[]{FlintRender::swap(0xf800)};
        CHECK(panel.writePixels(pixel) == ESP_OK);
        CHECK(s.gram.back() == 0xf800);
        CHECK(panel.writePixels(pixel) == ESP_ERR_INVALID_STATE);
        CHECK(panel.displayOn() == ESP_OK);
        CHECK(panel.setWindow(-1, 0, 1, 1) == ESP_ERR_INVALID_ARG);
        CHECK(panel.setWindow(0, 0, 0, 1) == ESP_ERR_INVALID_ARG);
        CHECK(panel.setWindow(INT_MAX, 0, 1, 1) == ESP_ERR_INVALID_ARG);
        CHECK(panel.setWindow(1, 1, INT_MAX, 1) == ESP_ERR_INVALID_ARG);
        CHECK(panel.setWindow(279, 239, 2, 1) == ESP_ERR_INVALID_ARG);
        CHECK(panel.setWindow(279, 239, 1, 2) == ESP_ERR_INVALID_ARG);
        CHECK(panel.setWindow(0, 0, 1, 1) == ESP_OK);
        CHECK(panel.writePixels({}) == ESP_ERR_INVALID_ARG);
        alignas(4) const uint16_t tooMany[2]{};
        CHECK(panel.writePixels(tooMany) == ESP_ERR_INVALID_ARG);
        s.failTransfer = s.attempts + 1;
        CHECK(panel.writePixels(pixel) == ESP_FAIL);
        CHECK(panel.writePixels(pixel) == ESP_ERR_INVALID_STATE);
        s.failTransfer = 0;
    }
    CHECK(!s.bus && s.busFrees == 1 && s.deviceRemovals == 1);
    for (int attempt = 1; attempt <= 27; ++attempt) {
        clearTransfers();
        s.failTransfer = attempt;
        flint::display::St7789 panel;
        CHECK(panel.begin() == ESP_FAIL);
        CHECK(!panel.ready() && !s.bus);
        s.failTransfer = 0;
        CHECK(panel.begin() == ESP_OK);
    }
    CHECK(!s.bus);
    s.failBus = true;
    const int frees = s.busFrees;
    {
        flint::display::St7789 panel;
        CHECK(panel.begin() == ESP_FAIL);
    }
    CHECK(s.busFrees == frees);
    s.failBus = false;
    s.failDevice = true;
    {
        flint::display::St7789 panel;
        CHECK(panel.begin() == ESP_FAIL);
    }
    CHECK(s.busFrees == frees + 1 && !s.bus);
    s.failDevice = false;
}
void displayTests() {
    auto &s = fake();
    clearTransfers();
    displayBegin();
    CHECK(s.pixelBytes == 280 * 240 * 2);
    CHECK(s.transfers.back().bytes == std::vector<uint8_t>{0x29});
    CHECK(std::all_of(s.gram.begin(), s.gram.end(), [](auto c) { return c == 0x0842; }));
    flint::Core core("", "", {});
    FlintRender::Scene scene;
    FlintRender::rect(scene, 0, 0, 280, 240, 0x0842);
    FlintRender::rect(scene, 17, 35, 11, 9, 0xf800);
    FlintRender::text(scene, 12, 90, "FlinT", 0xffff);
    std::array<uint16_t, 280 * 240> expected{};
    FlintRender::Canvas canvas(expected.data(), 280, 240);
    core.paint(scene, canvas);
    clearTransfers();
    const auto allocations = s.allocations;
    displayScene(core, scene);
    CHECK(s.gram == expected);
    CHECK(s.allocations == allocations);
    CHECK(s.pixelBytes == 280 * 240 * 2);
    clearTransfers();
    displayScene(core, scene);
    CHECK(s.transfers.empty());
    scene.ops[1].x = 19;
    const auto old = s.gram;
    FlintRender::Canvas changed(expected.data(), 280, 240);
    core.paint(scene, changed);
    clearTransfers();
    displayScene(core, scene);
    CHECK(s.gram == expected && s.gram != old);
    CHECK(s.pixelBytes == 13 * 9 * 2); // union of old and new odd-width rectangles
    CHECK(commandData(0x2a).bytes == (std::vector<uint8_t>{0, 37, 0, 49}));
    clearTransfers();
    displayRecovery("Test error");
    CHECK(s.allocations == allocations);
    CHECK(s.pixelBytes == 280 * 240 * 2);
    expected.fill(0x0842);
    FlintRender::Canvas recovery(expected.data(), 280, 240);
    const int ascent = FlintRender::be32(FlintRender::fontData(FlintRender::Body) + 16);
    for (const auto &[text, top] : {std::pair{"FlinT recovery", 50}, std::pair{"Test error", 90},
             std::pair{"Connect Flint Flare", 130}})
        recovery.label(12, top + ascent, reinterpret_cast<const uint8_t *>(text),
            std::char_traits<char>::length(text), 0xf800, FlintRender::Body, false);
    CHECK(s.gram == expected);
    clearTransfers();
    displayScene(core, scene);
    CHECK(s.pixelBytes == 280 * 240 * 2); // recovery forces full repaint
    CHECK(s.gram != expected);
    clearTransfers();
    scene.ops[1].color = 0x07e0;
    s.failTransfer = 6; // first pixel transfer, after CASET/RASET/RAMWR
    displayScene(core, scene);
    CHECK(s.pixelBytes == 280 * 240 * 2); // independent recovery after failed transfer
    s.failTransfer = 0;
    clearTransfers();
    displayScene(core, scene);
    CHECK(s.pixelBytes == 280 * 240 * 2); // failed frame was never cached
    clearTransfers();
    auto invalid = scene;
    invalid.width = 320;
    displayScene(core, invalid);
    CHECK(s.pixelBytes == 280 * 240 * 2);
    s.noPsram = true;
    displayBegin();
    CHECK(s.allocations == allocations + 2); // preferred PSRAM, then internal fallback
    s.failAllocation = true;
    displayBegin();
    const auto recoveryPixels = s.gram;
    CHECK(std::any_of(s.gram.begin(), s.gram.end(), [](auto c) { return c != 0x0842; }));
    clearTransfers();
    displayScene(core, scene);
    CHECK(s.transfers.empty() && s.gram == recoveryPixels);
    s.failAllocation = false;
    s.noPsram = false;
    displayBegin();
    clearTransfers();
    displayScene(core, scene);
    CHECK(s.pixelBytes == 280 * 240 * 2);

    // Exercise both union members repeatedly with a full scene payload. The
    // cached scene must survive unchanged frames and be rebuilt after DMA/recovery.
    FlintRender::Scene dense;
    FlintRender::rect(dense, 0, 0, 280, 240, 0x0842);
    for (int i = 1; i < FlintRender::MaxOps - 1; ++i)
        FlintRender::rect(dense, 8 * i, 8 + i, 7, 5, 0x07e0);
    std::array<uint8_t, FlintRender::MaxData> text;
    text.fill('A');
    FlintRender::Op label;
    label.kind = FlintRender::Text;
    label.y = 120;
    label.color = 0xffff;
    CHECK(dense.add(label, text.data(), text.size()));
    CHECK(dense.count == FlintRender::MaxOps && dense.used == FlintRender::MaxData);
    const int denseAllocations = s.allocations;
    for (int i = 0; i < 20; ++i) {
        dense.data[i] = uint8_t('B' + i);
        dense.ops[1].x = int16_t(8 + i);
        core.paint(dense, canvas);
        clearTransfers();
        displayScene(core, dense);
        CHECK(s.gram == expected);
        clearTransfers();
        displayScene(core, dense);
        CHECK(s.transfers.empty());
        if (i % 4 == 0) {
            displayRecovery("Test error");
            clearTransfers();
            displayScene(core, dense);
            CHECK(s.pixelBytes == 280 * 240 * 2 && s.gram == expected);
        }
        CHECK(s.allocations == denseAllocations);
    }
}
} // namespace

esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *config, int dma) {
    auto &s = fake();
    CHECK(host == SPI2_HOST && dma == SPI_DMA_CH_AUTO && !s.bus);
    if (s.failBus)
        return ESP_FAIL;
    s.bus = true;
    s.busConfig = *config;
    return ESP_OK;
}
esp_err_t spi_bus_add_device(
    int, const spi_device_interface_config_t *config, spi_device_handle_t *device) {
    if (fake().failDevice)
        return ESP_FAIL;
    fake().deviceConfig = *config;
    *device = new spi_device_t;
    return ESP_OK;
}
esp_err_t spi_bus_remove_device(spi_device_handle_t device) {
    ++fake().deviceRemovals;
    delete device;
    return ESP_OK;
}
esp_err_t spi_bus_free(int) {
    fake().bus = false;
    ++fake().busFrees;
    return ESP_OK;
}
esp_err_t spi_device_transmit(spi_device_handle_t, spi_transaction_t *transaction) {
    auto &s = fake();
    if (++s.attempts == s.failTransfer)
        return ESP_FAIL;
    const auto *bytes = (transaction->flags & SPI_TRANS_USE_TXDATA)
        ? transaction->tx_data
        : static_cast<const uint8_t *>(transaction->tx_buffer);
    const auto size = transaction->length / 8;
    CHECK(size && size <= 560 && transaction->length % 8 == 0);
    s.transfers.push_back({s.dc, {bytes, bytes + size}});
    if (!s.dc) {
        CHECK(size == 1);
        s.command = bytes[0];
        if (s.command == 0x2c) {
            s.x = s.x0;
            s.y = s.y0;
        }
    } else if (s.command == 0x2a || s.command == 0x2b) {
        CHECK(size == 4);
        const int first = (bytes[0] << 8) | bytes[1], last = (bytes[2] << 8) | bytes[3];
        if (s.command == 0x2a) {
            s.x0 = first;
            s.x1 = last;
        } else {
            s.y0 = first;
            s.y1 = last;
        }
    } else if (s.command == 0x2c) {
        CHECK(size % 2 == 0);
        s.pixelBytes += size;
        for (size_t i = 0; i < size; i += 2) {
            CHECK(s.x >= 20 && s.x < 300 && s.y >= 0 && s.y < 240 && s.y <= s.y1);
            s.gram[s.y * 280 + s.x - 20] = uint16_t((bytes[i] << 8) | bytes[i + 1]);
            if (++s.x > s.x1) {
                s.x = s.x0;
                ++s.y;
            }
        }
    }
    return ESP_OK;
}
esp_err_t gpio_config(const gpio_config_t *config) {
    CHECK(config->pin_bit_mask == ((1ULL << 4) | (1ULL << 5)));
    return ESP_OK;
}
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t value) {
    if (pin == FLINT_TFT_DC)
        fake().dc = value != 0;
    else
        CHECK(pin == FLINT_TFT_RST);
    return ESP_OK;
}
void vTaskDelay(TickType_t ticks) {
    fake().delays.push_back(ticks);
}
void *heap_caps_malloc(size_t size, uint32_t caps) {
    auto &s = fake();
    ++s.allocations;
    CHECK(size == 280 * 240 * 2);
    if (s.failAllocation || (s.noPsram && (caps & MALLOC_CAP_SPIRAM)))
        return nullptr;
    return std::malloc(size);
}
void heap_caps_free(void *buffer) {
    ++fake().frees;
    std::free(buffer);
}

int main() {
    try {
        driverTests();
        displayTests();
        std::cout
            << "ST7789 initialization, RGB565, dirty regions, recovery and failures: passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
