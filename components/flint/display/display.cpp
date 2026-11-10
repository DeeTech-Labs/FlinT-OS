#include "display.h"
#include "board/device_config.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "st7789.h"
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

namespace {
constexpr uint16_t Background = 0x0842, RecoveryColor = 0xf800;
constexpr int Width = FLINT_DESIGN_W, Height = FLINT_DESIGN_H;
flint::display::St7789 panel;
struct PixelDeleter {
    void operator()(uint16_t *buffer) const {
        heap_caps_free(buffer);
    }
};
std::unique_ptr<uint16_t, PixelDeleter> pixels;
// The old scene is consumed before any transfer starts. Reuse its storage for
// the DMA row, then reconstruct the scene only after the complete frame succeeds.
// This stays internal and available to recovery even without a framebuffer.
union DisplayScratch {
    FlintRender::Scene previous;
    std::array<uint16_t, FLINT_FB_MAX_W> row;
};
static_assert(Width <= FLINT_FB_MAX_W);
static_assert(std::is_trivially_copyable_v<FlintRender::Scene>);
static_assert(sizeof(DisplayScratch) == sizeof(FlintRender::Scene));
DMA_ATTR DisplayScratch scratch{.row = {}};
bool havePrevious = false;
uint64_t frames{}, recoveries{}, errors{}, maxRasterUs{}, maxCopyUs{}, maxSpiUs{};
uint64_t totalCopyUs{}, totalSpiUs{}, transferredPixels{}, pixelTransfers{};

uint16_t *beginRows() {
    havePrevious = false;
    // Default initialization intentionally leaves the row uninitialized. Every
    // transmitted pixel is filled or copied before use; no heap allocation occurs.
    auto *row = ::new (static_cast<void *>(&scratch.row)) std::array<uint16_t, FLINT_FB_MAX_W>;
    return row->data();
}

bool check(esp_err_t error) {
    if (error == ESP_OK)
        return true;
    ESP_LOGE("display", "%s", esp_err_to_name(error));
    ++errors;
    havePrevious = false;
    return false;
}

bool fillBackground() {
    auto *dmaRow = beginRows();
    std::fill_n(dmaRow, Width, FlintRender::swap(Background));
    if (!check(panel.setWindow(0, 0, Width, Height)))
        return false;
    for (int y = 0; y < Height; ++y)
        if (!check(panel.writePixels({dmaRow, Width})))
            return false;
    return true;
}

// Built-in SDK glyphs live in flash; no package, Core, or full framebuffer is needed.
void recoveryTextRow(FlintRender::Canvas &row, int y, const char *text, int top) {
    const auto *font = FlintRender::fontData(FlintRender::Body);
    const int ascent = FlintRender::be32(font + 16);
    const int lineHeight = ascent + FlintRender::be32(font + 20);
    int x = 12, baseline = top + ascent;
    const auto *bytes = reinterpret_cast<const uint8_t *>(text);
    const size_t length = std::strlen(text);
    for (size_t cursor = 0; cursor < length && baseline - ascent < Height;) {
        const size_t start = cursor;
        const auto cp = FlintRender::nextCodepoint(bytes, length, cursor);
        if (cp == '\n') {
            x = 0;
            baseline += lineHeight;
            continue;
        }
        const auto glyph = FlintRender::glyph(FlintRender::Body, cp);
        if (x + glyph.advance > Width) {
            x = 0;
            baseline += lineHeight;
        }
        row.label(x, baseline - y, bytes + start, cursor - start, RecoveryColor, FlintRender::Body,
            false);
        x += glyph.advance;
    }
}
} // namespace

void displayBegin() {
    havePrevious = false;
    pixels.reset();
    if (!check(panel.begin()) || !fillBackground() || !check(panel.displayOn()))
        return;
    // Prefer PSRAM when present. DMA staging always remains internal.
    pixels.reset(static_cast<uint16_t *>(
        heap_caps_malloc(Width * Height * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!pixels)
        pixels.reset(static_cast<uint16_t *>(
            heap_caps_malloc(Width * Height * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    if (!pixels)
        displayRecovery("Framebuffer unavailable");
}

void displayScene(flint::Core &core, const FlintRender::Scene &scene) {
    if (scene.width != Width || scene.height != Height) {
        displayRecovery("Unsupported canvas size");
        return;
    }
    if (!pixels || !panel.ready())
        return;
    const auto dirty = havePrevious ? FlintRender::dirty(scratch.previous, scene)
                                    : FlintRender::Bounds{0, 0, Width, Height};
    if (dirty.w <= 0 || dirty.h <= 0)
        return;
    FlintRender::Canvas canvas(pixels.get(), Width, Height, true);
    canvas.clip = dirty;
    const auto rasterStart = esp_timer_get_time();
    core.paint(scene, canvas);
    maxRasterUs = std::max(maxRasterUs, uint64_t(esp_timer_get_time() - rasterStart));
    ++frames;
    auto *dmaRow = beginRows();
    auto spiStart = esp_timer_get_time();
    if (!check(panel.setWindow(dirty.x, dirty.y, dirty.w, dirty.h))) {
        displayRecovery("Display transfer failed");
        return;
    }
    uint64_t spiUs = esp_timer_get_time() - spiStart, copyUs = 0;
    for (int y = dirty.y; y < dirty.y + dirty.h; ++y) {
        const auto copyStart = esp_timer_get_time();
        std::memcpy(dmaRow, pixels.get() + y * Width + dirty.x, dirty.w * 2);
        copyUs += esp_timer_get_time() - copyStart;
        spiStart = esp_timer_get_time();
        if (!check(panel.writePixels({dmaRow, size_t(dirty.w)}))) {
            displayRecovery("Display transfer failed");
            return;
        }
        spiUs += esp_timer_get_time() - spiStart;
        transferredPixels += dirty.w;
        ++pixelTransfers;
    }
    totalCopyUs += copyUs;
    totalSpiUs += spiUs;
    maxCopyUs = std::max(maxCopyUs, copyUs);
    maxSpiUs = std::max(maxSpiUs, spiUs);
    std::construct_at(&scratch.previous, scene);
    havePrevious = true;
}

void displayRecovery(const char *message) {
    ++recoveries;
    havePrevious = false;
    if (!panel.ready() || !check(panel.setWindow(0, 0, Width, Height)))
        return;
    auto *dmaRow = beginRows();
    for (int y = 0; y < Height; ++y) {
        std::fill_n(dmaRow, Width, FlintRender::swap(Background));
        FlintRender::Canvas row(dmaRow, Width, 1, true);
        recoveryTextRow(row, y, "FlinT recovery", 50);
        recoveryTextRow(row, y, message ? message : "", 90);
        recoveryTextRow(row, y, "Connect Flint Flare", 130);
        if (!check(panel.writePixels({dmaRow, Width})))
            return;
    }
}

void displayDiagnostics(flint::Values &values) {
    values["display_frames"] = std::to_string(frames);
    values["display_errors"] = std::to_string(errors);
    values["display_recoveries"] = std::to_string(recoveries);
    values["raster_max_us"] = std::to_string(maxRasterUs);
    values["copy_max_us"] = std::to_string(maxCopyUs);
    values["spi_max_us"] = std::to_string(maxSpiUs);
    values["copy_total_us"] = std::to_string(totalCopyUs);
    values["spi_total_us"] = std::to_string(totalSpiUs);
    values["display_pixels"] = std::to_string(transferredPixels);
    values["display_transfers"] = std::to_string(pixelTransfers);
}
