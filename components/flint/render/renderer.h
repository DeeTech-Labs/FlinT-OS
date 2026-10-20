#pragma once
#include "scene.h"

namespace FlintRender {
inline int32_t be32(const uint8_t *p) {
  return int32_t((uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
                 (uint32_t(p[2]) << 8) | p[3]);
}
struct Glyph {
  int width = 0, height = 0, advance = 0, ascent = 0, left = 0;
  const uint8_t *alpha = nullptr;
};
const uint8_t *fontData(unsigned font);
inline Glyph glyph(unsigned font, uint32_t cp) {
  const auto *data = fontData(font);
  const int n = be32(data);
  size_t offset = 24 + n * 28;
  for (int i = 0; i < n; ++i) {
    const auto *p = data + 24 + i * 28;
    const int h = be32(p + 4), w = be32(p + 8);
    if (uint32_t(be32(p)) == cp)
      return Glyph{
          w, h, be32(p + 12), be32(p + 16), be32(p + 20), data + offset};
    offset += size_t(h) * w;
  }
  if (cp != '?')
    return glyph(Body, '?');
  return {};
}
inline uint32_t nextCodepoint(const uint8_t *text, size_t n, size_t &cursor) {
  const auto first = text[cursor++];
  if (first < 128)
    return first;
  const unsigned count = (first & 0xe0) == 0xc0   ? 2
                         : (first & 0xf0) == 0xe0 ? 3
                         : (first & 0xf8) == 0xf0 ? 4
                                                  : 0;
  if (!count || cursor + count - 1 > n)
    return '?';
  uint32_t cp = first & ((1u << (7 - count)) - 1);
  for (unsigned i = 1; i < count; ++i) {
    const auto c = text[cursor];
    if ((c & 0xc0) != 0x80)
      return '?';
    ++cursor;
    cp = (cp << 6) | (c & 63);
  }
  return cp;
}
inline int textWidth(unsigned font, const uint8_t *text, size_t n) {
  int w = 0;
  size_t i = 0;
  while (i < n)
    w += glyph(font, nextCodepoint(text, n, i)).advance;
  return w;
}
struct Bounds {
  int x = 0, y = 0, w = 0, h = 0;
};
inline Bounds bounds(const Scene &s, const Op &op) {
  if (op.kind == Text) {
    const int w = textWidth(op.font, s.data + op.offset, op.length);
    const auto *f = fontData(op.font);
    return {op.x - (op.extra ? w / 2 : 0) - 4, op.y - be32(f + 16) - 2, w + 8,
            be32(f + 16) + be32(f + 20) + 4};
  }
  return {op.x, op.y, op.w, op.h};
}
inline Bounds dirty(const Scene &a, const Scene &b) {
  if (a.width != b.width || a.height != b.height || a.count != b.count)
    return {0, 0, b.width, b.height};
  Bounds result;
  bool any = false;
  for (int i = 0; i < b.count; ++i) {
    auto x = a.ops[i], y = b.ops[i];
    x.offset = y.offset = 0;
    if (memcmp(&x, &y, sizeof(Op)) == 0 && a.ops[i].length == b.ops[i].length &&
        !memcmp(a.data + a.ops[i].offset, b.data + b.ops[i].offset,
                b.ops[i].length))
      continue;
    for (const auto r : {bounds(a, a.ops[i]), bounds(b, b.ops[i])}) {
      if (!any) {
        result = r;
        any = true;
      } else {
        const int endX = std::max(result.x + result.w, r.x + r.w),
                  endY = std::max(result.y + result.h, r.y + r.h);
        result.x = std::min(result.x, r.x);
        result.y = std::min(result.y, r.y);
        result.w = endX - result.x;
        result.h = endY - result.y;
      }
    }
  }
  const int endX = std::min(int(b.width), result.x + result.w),
            endY = std::min(int(b.height), result.y + result.h);
  result.x = std::max(0, result.x);
  result.y = std::max(0, result.y);
  result.w = std::max(0, endX - result.x);
  result.h = std::max(0, endY - result.y);
  return result;
}
class Canvas {
public:
  uint16_t *pixels;
  int width, height;
  bool swapped;
  Bounds clip;
  Canvas(uint16_t *p, int w, int h, bool byteSwapped = false)
      : pixels(p), width(w), height(h), swapped(byteSwapped), clip{0, 0, w, h} {
  }
  uint16_t get(int x, int y) const {
    auto c = pixels[y * width + x];
    return swapped ? swap(c) : c;
  }
  void pixel(int x, int y, uint16_t c) {
    if (x >= clip.x && y >= clip.y && x < clip.x + clip.w &&
        y < clip.y + clip.h && x >= 0 && y >= 0 && x < width && y < height)
      pixels[y * width + x] = swapped ? swap(c) : c;
  }
  void fill(int x, int y, int w, int h, uint16_t c, int r = 0) {
    const int startX = std::max({0, x, clip.x}),
              endX = std::min({width, x + w, clip.x + clip.w});
    const int startY = std::max({0, y, clip.y}),
              endY = std::min({height, y + h, clip.y + clip.h});
    r = std::min(r, std::min(w, h) / 2);
    for (int yy = startY; yy < endY; ++yy)
      for (int xx = startX; xx < endX; ++xx) {
        if (r) {
          const int cx = xx < x + r        ? x + r - 1
                         : xx >= x + w - r ? x + w - r
                                           : xx,
                    cy = yy < y + r        ? y + r - 1
                         : yy >= y + h - r ? y + h - r
                                           : yy;
          if ((xx - cx) * (xx - cx) + (yy - cy) * (yy - cy) >= r * r)
            continue;
        }
        pixel(xx, yy, c);
      }
  }
  void label(int x, int baseline, const uint8_t *value, size_t n, uint16_t c,
             unsigned font, bool center) {
    if (center)
      x -= textWidth(font, value, n) / 2;
    size_t i = 0;
    while (i < n) {
      const auto g = glyph(font, nextCodepoint(value, n, i));
      for (int yy = 0; yy < g.height; ++yy) {
        const int py = baseline - g.ascent + yy;
        if (py < clip.y || py >= clip.y + clip.h || py < 0 || py >= height)
          continue;
        for (int xx = 0; xx < g.width; ++xx) {
          const int px = x + g.left + xx;
          if (px < clip.x || px >= clip.x + clip.w || px < 0 || px >= width)
            continue;
          const unsigned a = g.alpha[yy * g.width + xx];
          if (!a)
            continue;
          const auto bg = get(px, py);
          const unsigned r =
              (((c >> 11) & 31) * a + ((bg >> 11) & 31) * (255 - a) + 127) /
              255;
          const unsigned green =
              (((c >> 5) & 63) * a + ((bg >> 5) & 63) * (255 - a) + 127) / 255;
          const unsigned b = ((c & 31) * a + (bg & 31) * (255 - a) + 127) / 255;
          pixel(px, py, (r << 11) | (green << 5) | b);
        }
      }
      x += g.advance;
    }
  }
};
inline void render(const Scene &s, Canvas &canvas) {
  for (int i = 0; i < s.count; ++i) {
    const auto &op = s.ops[i];
    const auto b = bounds(s, op);
    if (b.x + b.w <= canvas.clip.x || b.y + b.h <= canvas.clip.y ||
        b.x >= canvas.clip.x + canvas.clip.w ||
        b.y >= canvas.clip.y + canvas.clip.h)
      continue;
    if (op.kind == Text)
      canvas.label(op.x, op.y, s.data + op.offset, op.length, op.color, op.font,
                   op.extra != 0);
    else if (op.kind == Bitmap) {
      for (int bit = 0; bit < 256; ++bit)
        if (s.data[op.offset + bit / 8] & (0x80 >> (bit % 8)))
          canvas.fill(op.x + (bit % 16) * op.extra,
                      op.y + (bit / 16) * op.extra, op.extra, op.extra,
                      op.color);
    } else
      canvas.fill(op.x, op.y, op.w, op.h, op.color,
                  op.kind == Rounded ? op.extra : 0);
  }
}
inline uint32_t pixelHash(const Canvas &canvas) {
  uint32_t h = 2166136261u;
  for (int y = 0; y < canvas.height; ++y)
    for (int x = 0; x < canvas.width; ++x) {
      const auto c = canvas.get(x, y);
      const uint8_t bytes[]{uint8_t(c >> 8), uint8_t(c)};
      h = hash(bytes, 2, h);
    }
  return h;
}
} // namespace FlintRender
