# Display impl factories

| File | Audience |
|------|----------|
| `lgfx_spi_stock.h` | **Default.** Common SPI TFT controllers via JSON `"panel"`. |
| `<custom>.h` | Only if the panel is not in the stock list (or needs a non-SPI bus). |

## Stock SPI panels (`lgfx_spi_stock`)

Chosen from common ESP32/Arduino SPI color-TFT usage (buying guides + TFT_eSPI /
DIYables driver coverage), mapped to LovyanGFX `Panel_*` classes:

| `"panel"` in JSON | Typical modules |
|-------------------|-----------------|
| `ILI9341` | 2.4″ / 2.8″ 240×320 (most common “Arduino TFT”) |
| `ST7789` | 1.3″–2.0″ IPS 240×240 / 240×280 / 240×320 |
| `ST7735` / `ST7735S` | 1.8″ 128×160 class |
| `ST7796` | ~3.5″–4.0″ SPI dashboards |
| `GC9A01` (`GC9A01A` alias) | 1.28″ round 240×240 wearables |
| `ILI9488` / `ILI9486` / `ILI9481` | 3.5″ 320×480 SPI |
| `ILI9342` | M5Stack-style 320×240 |
| `ILI9163` | older small 128×128 / 128×160 |

Normal board authors never add files here — only `boards/*.json` (+ `periph/` for
size/quirks of a known panel).
