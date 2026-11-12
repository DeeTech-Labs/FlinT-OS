# Fixed FlinT Spark S3 development hardware.
set(FLINT_DEVICE_ID "flint_spark_s3_supermini")
set(FLINT_DEVICE_IDF_TARGET "esp32s3")
set(FLINT_DEVICE_DISPLAY_ID "st7789_spi_240x280")
set(FLINT_DEVICE_DISPLAY_BUS "spi")
set(FLINT_DEVICE_DISPLAY_IMPL "spark_s3_display")
set(FLINT_DEVICE_CANVAS_STRATEGY "prefer_psram")
set(FLINT_DEVICE_IDF_REQUIRES "esp_driver_spi;esp_driver_gpio")
