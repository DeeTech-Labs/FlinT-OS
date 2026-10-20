#pragma once
#include "builders.h"
namespace FlintRender {
constexpr unsigned BootDurationMs = 1700;
inline unsigned bootProgress(unsigned t, unsigned start, unsigned duration) {
  if (t <= start)
    return 0;
  if (t >= start + duration)
    return 255;
  const unsigned u = 255 - (t - start) * 255 / duration;
  return 255 - u * u * u / (255 * 255);
}
inline uint16_t blend565(uint16_t a, uint16_t b, unsigned t) {
  return (((((a >> 11) & 31) * (255 - t) + ((b >> 11) & 31) * t + 127) / 255)
          << 11) |
         (((((a >> 5) & 63) * (255 - t) + ((b >> 5) & 63) * t + 127) / 255)
          << 5) |
         (((a & 31) * (255 - t) + (b & 31) * t + 127) / 255);
}
inline Scene bootScene(unsigned elapsed) {
  Scene s;
  rect(s, 0, 0, 280, 240, Background);
  const auto reveal = bootProgress(elapsed, 80, 540);
  const auto exit = 255 - bootProgress(elapsed, 1510, 190);
  const auto color = [&](uint16_t value, unsigned alpha) {
    return blend565(Background, value, alpha * exit / 255);
  };
  const int rise = (255 - reveal) * 7 / 255, cy = 87 + rise;
  // A restrained blue halo and a floating enamel tile, all in the existing
  // buffer.
  rect(s, 92, cy - 48, 96, 96, color(0x08a7, reveal), 25);
  rect(s, 97, cy - 43, 86, 86, color(0x112b, reveal), 22);
  rect(s, 101, cy - 39, 78, 78, color(0x192e, reveal), 19);
  const auto mark = bootProgress(elapsed, 200, 530);
  const auto ink = color(White, mark), blue = color(Accent, mark),
             inside = color(0x192e, reveal);
  rect(s, 129, cy - 32, 22, 9, blue, 3);
  rect(s, 129, cy + 23, 22, 9, blue, 3);
  rect(s, 119, cy - 25, 42, 50, ink, 11);
  rect(s, 122, cy - 22, 36, 44, inside, 8);
  rect(s, 160, cy - 9, 3, 10, blue, 1);
  rect(s, 138, cy - 12, 3, 14, ink, 1);
  rect(s, 138, cy - 1, 12, 3, ink, 1);
  rect(s, 134, cy - 3, 10, 7, blue, 3);
  text(s, 140, 165, "FlinT", color(White, bootProgress(elapsed, 390, 500)),
       Brand, true);
  text(s, 140, 192, "DeeTech Labs",
       color(Muted, bootProgress(elapsed, 650, 450)), Body, true);
  // A short light sweep is decoration, never a fabricated loading percentage.
  const auto sweep = bootProgress(elapsed, 760, 690);
  rect(s, 106, 214, 68, 2, color(0x194e, mark), 1);
  if (elapsed >= 760 && elapsed < 1450) {
    const int x = 106 + int(sweep) * 52 / 255;
    const auto pulse = elapsed < 1120 ? bootProgress(elapsed, 760, 360)
                                      : 255 - bootProgress(elapsed, 1120, 330);
    rect(s, x, 214, 16, 2, color(Accent, pulse), 1);
  }
  return s;
}
} // namespace FlintRender
