#pragma once

// Stock SPI panel factory — panel class from FLINT_PANEL_KIND_* (periph JSON "panel").
//
// Controllers chosen from common ESP32/Arduino SPI TFT usage (2024–2026 guides +
// TFT_eSPI driver set), not an exhaustive LovyanGFX dump:
//   ILI9341, ST7789, ST7735/ST7735S, ST7796, GC9A01, ILI9486/ILI9488, ILI9342, ILI9163
// Aliases: GC9A01A → GC9A01.
//
// Community boards: JSON only. Custom impl/*.h only for unsupported panels/buses.

namespace flint_lgfx {

#if defined(FLINT_PANEL_KIND_ST7789)
#if FLINT_PANEL_DEFER_DISPON
// Stock ST7789 init ends with DISPON while GRAM is still empty/zero.
// Defer DISPON until after the first fill (see displayBegin).
struct Panel_ST7789_NoDispOn : public lgfx::Panel_ST7789 {
 protected:
  const uint8_t* getInitCommands(uint8_t listno) const override {
    static constexpr uint8_t list0[] = {
        CMD_GCTRL, 1, 0x35,
        CMD_VCOMS, 1, 0x28,
        CMD_LCMCTRL, 1, 0x0C,
        CMD_VDVVRHEN, 2, 0x01, 0xFF,
        CMD_VRHS, 1, 0x10,
        CMD_VDVSET, 1, 0x20,
        CMD_PWCTRL1, 2, 0xa4, 0xa1,
        CMD_RAMCTRL, 2, 0x00, 0xC0,
        CMD_PVGAMCTRL, 14, 0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x32, 0x44, 0x42, 0x06,
        0x0e, 0x12, 0x14, 0x17,
        CMD_NVGAMCTRL, 14, 0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x31, 0x54, 0x47, 0x0e,
        0x1c, 0x17, 0x1b, 0x1e,
        CMD_SLPOUT, 0 + CMD_INIT_DELAY, 130,
        CMD_IDMOFF, 0,
        // CMD_DISPON omitted on purpose
        0xFF, 0xFF,
    };
    switch (listno) {
      case 0:
        return list0;
      default:
        return nullptr;
    }
  }
};
using PanelType = Panel_ST7789_NoDispOn;
#else
using PanelType = lgfx::Panel_ST7789;
#endif

#elif defined(FLINT_PANEL_KIND_ILI9341)
using PanelType = lgfx::Panel_ILI9341;

#elif defined(FLINT_PANEL_KIND_ILI9342)
using PanelType = lgfx::Panel_ILI9342;

#elif defined(FLINT_PANEL_KIND_ST7735)
using PanelType = lgfx::Panel_ST7735;

#elif defined(FLINT_PANEL_KIND_ST7735S)
using PanelType = lgfx::Panel_ST7735S;

#elif defined(FLINT_PANEL_KIND_ST7796)
using PanelType = lgfx::Panel_ST7796;

#elif defined(FLINT_PANEL_KIND_GC9A01)
using PanelType = lgfx::Panel_GC9A01;

#elif defined(FLINT_PANEL_KIND_ILI9488)
using PanelType = lgfx::Panel_ILI9488;

#elif defined(FLINT_PANEL_KIND_ILI9486)
using PanelType = lgfx::Panel_ILI9486;

#elif defined(FLINT_PANEL_KIND_ILI9481)
using PanelType = lgfx::Panel_ILI9481;

#elif defined(FLINT_PANEL_KIND_ILI9163)
using PanelType = lgfx::Panel_ILI9163;

#else
#error \
    "Unknown SPI panel for lgfx_spi_stock — set periph \"panel\" to a supported id " \
    "(ILI9341, ST7789, ST7735, ST7735S, ST7796, GC9A01, ILI9488, ILI9486, ILI9481, " \
    "ILI9342, ILI9163) or add board/impl/<custom>.h"
#endif

}  // namespace flint_lgfx

class LGFX : public lgfx::LGFX_Device {
  flint_lgfx::PanelType _panel_instance;
  lgfx::Bus_SPI _bus_instance;

 public:
  LGFX() {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = FLINT_SPI_FREQ_WRITE;
      cfg.freq_read = FLINT_SPI_FREQ_READ;
      cfg.spi_3wire = FLINT_SPI_3WIRE;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = FLINT_TFT_SCLK;
      cfg.pin_mosi = FLINT_TFT_MOSI;
      cfg.pin_miso = FLINT_TFT_MISO;
      cfg.pin_dc = FLINT_TFT_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = FLINT_TFT_CS;
      cfg.pin_rst = FLINT_TFT_RST;
      cfg.pin_busy = FLINT_TFT_BUSY;
      cfg.panel_width = FLINT_PANEL_W;
      cfg.panel_height = FLINT_PANEL_H;
      cfg.offset_x = FLINT_PANEL_OFFSET_X;
      cfg.offset_y = FLINT_PANEL_OFFSET_Y;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = FLINT_PANEL_READABLE;
      cfg.invert = FLINT_PANEL_INVERT;
      cfg.rgb_order = FLINT_PANEL_RGB_ORDER;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};
