#pragma once

// FlinT OS — LovyanGFX board config for ESP32-C3 + ST7789 (240×280)
// Pins from config/config.h

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "config/config.h"

namespace flint_lgfx {

// Stock ST7789 init ends with DISPON while GRAM is still empty/zero.
// With panel invert that reads as a white flash before we can paint.
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

}  // namespace flint_lgfx

class LGFX : public lgfx::LGFX_Device {
  flint_lgfx::Panel_ST7789_NoDispOn _panel_instance;
  lgfx::Bus_SPI _bus_instance;

 public:
  LGFX() {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = TFT_SCLK;
      cfg.pin_mosi = TFT_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = TFT_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = TFT_CS;
      cfg.pin_rst = TFT_RST;
      cfg.pin_busy = -1;
      // Glass 240×280; Panel_ST7789 already defaults GRAM to 240×320 (ST7789V3).
      cfg.panel_width = 240;
      cfg.panel_height = 280;
      cfg.offset_x = 0;
      cfg.offset_y = 20;  // center 280 in 320; try 0/12 if your module differs
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};
