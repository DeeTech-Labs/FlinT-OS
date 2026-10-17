/* Vendored u8g2 font subsets (olikraus/u8g2, BSD license) for LovyanGFX U8g2font.
 * Full U8g2 library is NOT required — only these blobs.
 *
 * All *.c fonts use brace-init uint8_t arrays (not C string literals) so GCC
 * -Werror=unterminated-string-initialization does not fire on sized blobs.
 *
 * u8g2_font_10x20_t_cyrillic.c — printable ASCII + Cyrillic glyphs used by
 * FlinT OS en_US/ru_RU i18n strings and chrome (not the full upstream set).
 * Re-subset after adding RU/EN UI strings that introduce new codepoints.
 */

Active UI fonts: antialiased Lato subsets in flint_*.inc (SIL OFL 1.1).
Large time: Lato Light 80px; compact time: Lato Light 62px; body: Lato Regular 20px.
Only required glyphs are embedded. See Lato-LICENSE.txt for attribution/license.
Regenerate with Pillow and original Lato TTF files:
  python3 tools/gen_clock_fonts.py --font-dir /path/to/lato
Source hashes are recorded in generated files. Old u8g2 blobs are retained
for attribution/history but are no longer linked into firmware.
