// Fixed FlinT Spark S3 development hardware.
// Board: flint_spark  display: st7789_spi_240x280  impl: spark_s3_display
#pragma once

#include <stdint.h>

inline constexpr const char *FLINT_MODEL_ID = "flint_spark";
inline constexpr const char *FLINT_HW_REVISION = "s3_supermini";
inline constexpr const char *FLINT_HW_STAGE = "development";
#define FLINT_HAS_BUTTONS 0
#define FLINT_HAS_BATTERY 0
#define FLINT_HAS_HAPTICS 0
#define FLINT_HAS_BACKLIGHT_CONTROL 0
inline constexpr const char *FLINT_DEVICE_ID = "flint_spark_s3_supermini";
inline constexpr const char *FLINT_IDF_TARGET = "esp32s3";
inline constexpr const char *FLINT_DISPLAY_ID = "st7789_spi_240x280";
inline constexpr const char *FLINT_DISPLAY_PANEL = "ST7789";
inline constexpr const char *FLINT_DISPLAY_BUS = "spi";
inline constexpr const char *FLINT_DISPLAY_IMPL = "spark_s3_display";
inline constexpr const char *FLINT_CANVAS_STRATEGY = "prefer_psram";
inline constexpr const char *FLINT_LAYOUT = "280x240_landscape";

#define FLINT_DISPLAY_IMPL_spark_s3_display 1
#define FLINT_PANEL_KIND_ST7789 1
#define FLINT_HAS_TOUCH 0

inline constexpr int FLINT_STATUS_LED_GPIO = -1;
inline constexpr bool FLINT_STATUS_LED_ACTIVE_LOW = true;

inline constexpr int FLINT_TFT_CS = 10;
inline constexpr int FLINT_TFT_DC = 4;
inline constexpr int FLINT_TFT_RST = 5;
inline constexpr int FLINT_TFT_MOSI = 11;
inline constexpr int FLINT_TFT_SCLK = 12;
inline constexpr int FLINT_TFT_MISO = -1;
inline constexpr int FLINT_TFT_BUSY = -1;
inline constexpr int FLINT_TFT_BL = -1;

inline constexpr uint8_t FLINT_TFT_ROTATION = 3;
inline constexpr int FLINT_PANEL_W = 240;
inline constexpr int FLINT_PANEL_H = 280;
inline constexpr int FLINT_PANEL_OFFSET_X = 0;
inline constexpr int FLINT_PANEL_OFFSET_Y = 20;
inline constexpr bool FLINT_PANEL_INVERT = true;
inline constexpr bool FLINT_PANEL_RGB_ORDER = false;
inline constexpr bool FLINT_SPI_3WIRE = true;
inline constexpr uint32_t FLINT_SPI_FREQ_WRITE = 80000000u;

inline constexpr int FLINT_FB_MAX_W = 280;
inline constexpr int FLINT_FB_MAX_H = 240;
inline constexpr int FLINT_DESIGN_W = 280;
inline constexpr int FLINT_DESIGN_H = 240;
inline constexpr int FLINT_SAFE_INSET = 16;
