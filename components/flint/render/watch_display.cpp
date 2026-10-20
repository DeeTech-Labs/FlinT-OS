#include "render/watch_display.h"
#include "display/display.h"
#include "render/boot.h"
#include "render/version.h"
#include "usb/pc_data.h"
#include "util/flint_sys.h"
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

uint16_t *displayPixelBuffer();
namespace {
FlintRender::Scene committed, pending;
bool fadeNext = false;
bool available = false, internalPresent = false, pinned = false,
     havePending = false;
uint32_t revision = 0, sceneHash = 0, pinStarted = 0;
uint8_t encoded[FlintRender::MaxEncoded];
size_t encodedSize = 0;
uint8_t transferEncoded[FlintRender::MaxEncoded];
size_t transferSize = 0;
uint32_t transferRevision = 0;
bool number(const char *text, uint32_t &n) {
  n = 0;
  if (!text || !*text)
    return false;
  for (const char *p = text; *p; ++p) {
    if (*p < '0' || *p > '9' || n > (UINT32_MAX - unsigned(*p - '0')) / 10)
      return false;
    n = n * 10 + *p - '0';
  }
  return true;
}
uint32_t pixelsHash() {
  FlintRender::Canvas canvas(displayPixelBuffer(), SCR_W, SCR_H, true);
  return FlintRender::pixelHash(canvas);
}
void unpin() {
  pinned = false;
  if (havePending) {
    havePending = false;
    watchPresentScene(pending);
  }
}
} // namespace
void watchFadeInNextScene() { fadeNext = true; }
void watchPresentScene(const FlintRender::Scene &scene) {
  if (!scene.valid || scene.width != SCR_W || scene.height != SCR_H ||
      !displayHasCanvas()) {
    available = false;
    return;
  }
  if (pinned) {
    pending = scene;
    havePending = true;
    return;
  }
  if (fadeNext) {
    fadeNext = false;
    for (unsigned alpha : {32u, 96u, 170u, 225u}) {
      auto frame = scene;
      for (int i = 0; i < frame.count; ++i)
        frame.ops[i].color = FlintRender::blend565(FlintRender::Background,
                                                   frame.ops[i].color, alpha);
      watchPresentScene(frame);
      flintDelay(30);
    }
  }
  uint8_t bytes[FlintRender::MaxEncoded];
  const auto size = FlintRender::encode(scene, bytes, sizeof(bytes));
  if (!size) {
    available = false;
    return;
  }
  if (available && size == encodedSize && !memcmp(bytes, encoded, size))
    return;
  const auto area = available ? FlintRender::dirty(committed, scene)
                              : FlintRender::Bounds{0, 0, SCR_W, SCR_H};
  FlintRender::Canvas canvas(displayPixelBuffer(), SCR_W, SCR_H, true);
  canvas.clip = area;
  FlintRender::render(scene, canvas);
  internalPresent = true;
  if (area.w > 0 && area.h > 0)
    displayPresentRect(area.x, area.y, area.w, area.h);
  internalPresent = false;
  committed = scene;
  memcpy(encoded, bytes, size);
  encodedSize = size;
  sceneHash = FlintRender::hash(encoded, size);
  ++revision;
  available = true;
}
void watchDisplayPresented() {
  if (!internalPresent) {
    available = false;
    ++revision;
  }
}
void watchDisplayMaintain() {
  if (pinned && uint32_t(flintMillis() - pinStarted) > 30000)
    unpin();
}
bool watchDisplayCommand(const char *operation, const char *arguments,
                         char *json, size_t capacity) {
  if (!strcmp(operation, "SCREEN")) {
    uint32_t known = 0;
    if (*arguments && !number(arguments, known))
      return false;
    if (*arguments && known == revision) {
      snprintf(json, capacity, "{\"revision\":%lu,\"unchanged\":true}",
               static_cast<unsigned long>(revision));
      return true;
    }
    if (available) {
      memcpy(transferEncoded, encoded, encodedSize);
      transferSize = encodedSize;
      transferRevision = revision;
    }
    PcJsonWriter w(json, capacity);
    w.raw("{\"renderer\":");
    w.text(FlintRender::RendererId);
    w.raw(",\"revision\":");
    w.number(revision);
    w.raw(",\"available\":");
    w.boolean(available);
    w.raw(",\"width\":");
    w.number(SCR_W);
    w.raw(",\"height\":");
    w.number(SCR_H);
    w.raw(",\"bytes\":");
    w.number(available ? encodedSize : 0);
    w.raw(",\"hash\":");
    w.number(sceneHash);
    w.raw(",\"unchanged\":");
    w.boolean(*arguments && known == revision);
    w.raw("}");
    return w.finish();
  }
  if (!strcmp(operation, "SHOT") && !strcmp(arguments, "stop")) {
    unpin();
    snprintf(json, capacity, "{\"released\":true}");
    return true;
  }
  if (!available && strcmp(operation, "SCENE"))
    return false;
  if (!strcmp(operation, "SHOT")) {
    if (!strcmp(arguments, "start")) {
      pinned = true;
      pinStarted = flintMillis();
    } else if (!strcmp(arguments, "stop")) {
      unpin();
      strcpy(json, "{\"released\":true}");
      return true;
    } else
      return false;
    snprintf(json, capacity, "{\"revision\":%lu,\"hash\":%lu,\"pixels\":%d}",
             static_cast<unsigned long>(revision),
             static_cast<unsigned long>(pixelsHash()), SCR_W * SCR_H);
    return true;
  }
  if (!strcmp(operation, "SCENE") || !strcmp(operation, "PIXELS")) {
    char args[40];
    if (strlen(arguments) >= sizeof(args))
      return false;
    strcpy(args, arguments);
    char *split = strchr(args, ' ');
    if (!split)
      return false;
    *split++ = 0;
    uint32_t requested, offset;
    if (!number(args, requested) || !number(split, offset))
      return false;
    const bool pixels = !strcmp(operation, "PIXELS");
    if (pixels && (!pinned || requested != revision))
      return false;
    if (!pixels && requested != transferRevision)
      return false;
    const size_t length = pixels ? size_t(SCR_W) * SCR_H * 2 : transferSize;
    if (offset >= length || (pixels && offset % 2))
      return false;
    const auto count = std::min<size_t>(224, length - offset);
    char hex[449];
    constexpr char alphabet[] = "0123456789abcdef";
    for (size_t i = 0; i < count; ++i) {
      uint8_t b;
      if (pixels) {
        const auto value =
            FlintRender::swap(displayPixelBuffer()[(offset + i) / 2]);
        b = ((offset + i) % 2) ? value : uint8_t(value >> 8);
      } else
        b = transferEncoded[offset + i];
      hex[i * 2] = alphabet[b >> 4];
      hex[i * 2 + 1] = alphabet[b & 15];
    }
    hex[count * 2] = 0;
    snprintf(json, capacity,
             "{\"revision\":%lu,\"offset\":%lu,\"data\":\"%s\"}",
             static_cast<unsigned long>(requested),
             static_cast<unsigned long>(offset), hex);
    return true;
  }
  return false;
}
