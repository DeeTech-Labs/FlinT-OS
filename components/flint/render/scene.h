#pragma once
#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace FlintRender {
constexpr int MaxOps = 32, MaxData = 512, HeaderSize = 10, OpSize = 18;
constexpr size_t MaxEncoded = HeaderSize + MaxOps * OpSize + MaxData;
enum Kind : uint8_t { Rect = 1, Rounded = 2, Line = 3, Text = 4, Bitmap = 5 };
enum Font : uint8_t { Body = 0, TimeLarge = 1, TimeCompact = 2, Brand = 3 };
struct Op {
  uint8_t kind = 0, font = 0;
  int16_t x = 0, y = 0, w = 0, h = 0;
  uint16_t color = 0, offset = 0, length = 0, extra = 0;
};
struct Scene {
  uint16_t width = 280, height = 240, used = 0;
  uint8_t count = 0;
  bool valid = true;
  Op ops[MaxOps]{};
  uint8_t data[MaxData]{};
  bool add(Op op, const void *bytes = nullptr, size_t length = 0) {
    if (!valid || count >= MaxOps || used + length > MaxData) {
      valid = false;
      return false;
    }
    op.offset = used;
    op.length = length;
    if (length)
      memcpy(data + used, bytes, length);
    used += length;
    ops[count++] = op;
    return true;
  }
};
inline uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
inline uint16_t rgb24(uint32_t c) {
  return rgb((c >> 16) & 255, (c >> 8) & 255, c & 255);
}
inline uint16_t swap(uint16_t c) { return uint16_t((c << 8) | (c >> 8)); }
inline void put16(uint8_t *p, uint16_t n) {
  p[0] = n >> 8;
  p[1] = n;
}
inline uint16_t get16(const uint8_t *p) { return uint16_t((p[0] << 8) | p[1]); }
inline size_t encode(const Scene &s, uint8_t *out, size_t capacity) {
  const size_t size = HeaderSize + s.count * OpSize + s.used;
  if (!s.valid || size > capacity || s.count > MaxOps || s.used > MaxData)
    return 0;
  out[0] = 'F';
  out[1] = 'S';
  out[2] = 1;
  out[3] = s.count;
  put16(out + 4, s.width);
  put16(out + 6, s.height);
  put16(out + 8, s.used);
  for (int i = 0; i < s.count; ++i) {
    const auto &op = s.ops[i];
    auto *p = out + HeaderSize + i * OpSize;
    p[0] = op.kind;
    p[1] = op.font;
    put16(p + 2, op.x);
    put16(p + 4, op.y);
    put16(p + 6, op.w);
    put16(p + 8, op.h);
    put16(p + 10, op.color);
    put16(p + 12, op.offset);
    put16(p + 14, op.length);
    put16(p + 16, op.extra);
  }
  memcpy(out + HeaderSize + s.count * OpSize, s.data, s.used);
  return size;
}
inline bool decode(const uint8_t *bytes, size_t size, Scene &s) {
  if (size < HeaderSize || bytes[0] != 'F' || bytes[1] != 'S' || bytes[2] != 1)
    return false;
  Scene next;
  next.count = bytes[3];
  next.width = get16(bytes + 4);
  next.height = get16(bytes + 6);
  next.used = get16(bytes + 8);
  if (next.count > MaxOps || next.used > MaxData || next.width < 1 ||
      next.width > 320 || next.height < 1 || next.height > 320 ||
      size != size_t(HeaderSize + next.count * OpSize + next.used))
    return false;
  for (int i = 0; i < next.count; ++i) {
    const auto *p = bytes + HeaderSize + i * OpSize;
    auto &op = next.ops[i];
    op.kind = p[0];
    op.font = p[1];
    op.x = int16_t(get16(p + 2));
    op.y = int16_t(get16(p + 4));
    op.w = int16_t(get16(p + 6));
    op.h = int16_t(get16(p + 8));
    op.color = get16(p + 10);
    op.offset = get16(p + 12);
    op.length = get16(p + 14);
    op.extra = get16(p + 16);
    if (op.kind < Rect || op.kind > Bitmap || op.font > Brand ||
        unsigned(op.offset) + op.length > next.used || op.x < -320 ||
        op.y < -320 || op.x > 640 || op.y > 640 || op.w < 0 || op.w > 640 ||
        op.h < 0 || op.h > 640)
      return false;
    if (op.kind == Bitmap && (op.length != 32 || op.extra < 1 || op.extra > 6 ||
                              op.w != 16 * op.extra || op.h != 16 * op.extra))
      return false;
    if (op.kind == Text && (op.length > 192 || op.extra > 1))
      return false;
  }
  if (!next.count || next.ops[0].kind != Rect || next.ops[0].x ||
      next.ops[0].y || next.ops[0].w != next.width ||
      next.ops[0].h != next.height)
    return false;
  memcpy(next.data, bytes + HeaderSize + next.count * OpSize, next.used);
  s = next;
  return true;
}
inline uint32_t hash(const uint8_t *data, size_t n, uint32_t h = 2166136261u) {
  for (size_t i = 0; i < n; ++i)
    h = (h ^ data[i]) * 16777619u;
  return h;
}
inline void rect(Scene &s, int x, int y, int w, int h, uint16_t color,
                 int radius = 0) {
  Op op;
  op.kind = radius ? Rounded : Rect;
  op.x = x;
  op.y = y;
  op.w = w;
  op.h = h;
  op.color = color;
  op.extra = radius;
  s.add(op);
}
inline void text(Scene &s, int x, int baseline, const char *value,
                 uint16_t color, Font font = Body, bool centered = false) {
  Op op;
  op.kind = Text;
  op.x = x;
  op.y = baseline;
  op.color = color;
  op.font = font;
  op.extra = centered ? 1 : 0;
  s.add(op, value, strlen(value));
}
inline void bitmap(Scene &s, int x, int y, const uint8_t *bits, int scale,
                   uint16_t color) {
  Op op;
  op.kind = Bitmap;
  op.x = x;
  op.y = y;
  op.w = op.h = 16 * scale;
  op.extra = scale;
  op.color = color;
  s.add(op, bits, 32);
}
} // namespace FlintRender
