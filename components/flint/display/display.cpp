#include "display.h"
#include "esp_heap_caps.h"
#include <cstring>
static LGFX panel;
static uint16_t *pixels;
static uint16_t *dmaRow;
static FlintRender::Scene previous;
static bool havePrevious = false;
void displayBegin() {
    panel.init();
    panel.setRotation(FLINT_TFT_ROTATION);
    panel.setColorDepth(16);
    panel.fillScreen(0x0842);
#if FLINT_PANEL_DEFER_DISPON
    panel.startWrite();
    panel.writeCommand(0x29);
    panel.endWrite();
#endif
    // Prefer PSRAM when present. DMA staging always remains internal.
    pixels = static_cast<uint16_t *>(
        heap_caps_malloc(280 * 240 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pixels)
        pixels = static_cast<uint16_t *>(
            heap_caps_malloc(280 * 240 * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    dmaRow =
        static_cast<uint16_t *>(heap_caps_malloc(280 * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (!pixels || !dmaRow)
        displayRecovery("Framebuffer unavailable");
}
void displayScene(flint::Core &core, const FlintRender::Scene &scene) {
    if (scene.width != 280 || scene.height != 240) {
        displayRecovery("Unsupported canvas size");
        return;
    }
    if (!pixels || !dmaRow)
        return;
    auto dirty =
        havePrevious ? FlintRender::dirty(previous, scene) : FlintRender::Bounds{0, 0, 280, 240};
    if (dirty.w <= 0 || dirty.h <= 0)
        return;
    FlintRender::Canvas canvas(pixels, 280, 240, true);
    canvas.clip = dirty;
    core.paint(scene, canvas);
    panel.startWrite();
    panel.setWindow(dirty.x, dirty.y, dirty.x + dirty.w - 1, dirty.y + dirty.h - 1);
    for (int y = dirty.y; y < dirty.y + dirty.h; ++y) {
        memcpy(dmaRow, pixels + y * 280 + dirty.x, dirty.w * 2);
        panel.writePixelsDMA(dmaRow, dirty.w);
        panel.waitDMA();
    }
    panel.endWrite();
    previous = scene;
    havePrevious = true;
}
void displayRecovery(const char *message) {
    havePrevious = false;
    panel.fillScreen(0x0842);
    panel.setTextColor(0xf800);
    panel.setFont(&lgfx::v1::fonts::Font2);
    panel.setCursor(12, 50);
    panel.print("FlinT recovery");
    panel.setCursor(12, 90);
    panel.print(message);
    panel.setCursor(12, 130);
    panel.print("Connect FlinT Desktop");
}
