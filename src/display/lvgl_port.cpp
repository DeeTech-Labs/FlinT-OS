#include "lvgl_port.h"

#if FLINT_ENABLE_LVGL

#include "display.h"
#include "src/config/debug.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <esp_heap_caps.h>

// Partial buffers: ~20 lines × width × RGB565. Keep small for ESP32-C3 + face canvas.
static const uint16_t kBufLines = 20;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;
static lv_disp_drv_t disp_drv;
static bool gActive = false;
static bool gReady = false;

static void lvglFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;

  if (tft.getStartCount() == 0) {
    tft.startWrite();
  }
  tft.pushImageDMA(area->x1, area->y1, w, h,
                   reinterpret_cast<lgfx::rgb565_t*>(&color_p->full));
  lv_disp_flush_ready(disp);
}

void lvglPortBegin() {
  lv_init();

  const size_t pixels = static_cast<size_t>(SCR_W) * kBufLines;
  const size_t bytes = pixels * sizeof(lv_color_t);

  buf1 = static_cast<lv_color_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  buf2 = static_cast<lv_color_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));

  if (!buf1) {
    buf1 = static_cast<lv_color_t*>(malloc(bytes));
  }
  if (!buf2) {
    buf2 = static_cast<lv_color_t*>(malloc(bytes));
  }

  if (!buf1) {
    LOGE("lvgl", "draw buffer alloc failed");
    return;
  }

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, pixels);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCR_W;
  disp_drv.ver_res = SCR_H;
  disp_drv.flush_cb = lvglFlush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  gReady = true;
  gActive = false;
  LOGI("lvgl", "ready %dx%d buf_lines=%u heap=%u", SCR_W, SCR_H,
       (unsigned)kBufLines, (unsigned)ESP.getFreeHeap());
}

void lvglPortTick(uint32_t ms) {
  (void)ms;
}

void lvglPortHandler() {
  if (!gReady || !gActive) return;
  lv_timer_handler();
}

void lvglPortSetActive(bool on) {
  gActive = on;
}

bool lvglPortIsActive() {
  return gActive;
}

#endif  // FLINT_ENABLE_LVGL
