[Русский оригинал](../../ru/architecture/display.md)

# Display and recovery

The display uses a native ST7789 driver (`display/st7789.cpp`) over ESP-IDF SPI Master. Power/gamma commands, orientation and offsets belong to the board profile. The driver is owned by the main application task; synchronous SPI/DMA completion allows immediate reuse of the scanline buffer. Initialization and transfer errors propagate to the caller, and partially initialized resources are released.

The IgniteSDK renderer and dirty-region calculation are preserved. The RGB565 framebuffer needs 134,400 bytes, preferring PSRAM with an internal-RAM fallback. The previous scene and the 560-byte DMA scanline share 1352 bytes of static internal storage. The scene is read for dirty-region calculation; its lifetime then ends and the scanline becomes active. The scene is reconstructed only after all transfers succeed. This saves a separate 560-byte buffer without new allocations, additional SPI transfers or changing ISR placement in IRAM. Initial fill and recovery use the same storage independently of the framebuffer. `DISPON` follows the initial background fill. Recovery uses IgniteSDK's built-in `Body` font without packages. Its redesigned layout separates a title, bounded failure reason and USB/Flare recovery instructions, using colors aligned with the base screens. It needs no full framebuffer, filesystem or Core instance.

CMake enables `CONFIG_COMPILER_OPTIMIZATION_CHECKS_SILENT` only for `esp_driver_spi`. Argument checks and error returns remain enabled; detailed `ESP_RETURN_ON_*` and `ESP_GOTO_ON_*` messages inside this component are omitted. Assertions, ISR placement in IRAM, other SPI messages and diagnostics in other components are unchanged. FlinT still handles and logs returned errors. This reduces IRAM validation code at the cost of less detailed low-level SPI diagnostics.

Host driver checks (SPI/GPIO simulation, no device access):

```sh
cmake -S tests/host -B /tmp/flint-display -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/flint-display
ctest --test-dir /tmp/flint-display --output-on-failure
```

The tests compare pixels against the portable renderer and cover addressing, panel settings, dirty regions, transfer failures, resource cleanup and recovery after allocation failure. They do not verify SPI electrical behavior, physical orientation, tearing or device performance. Lua, LittleFS and package formats are unchanged.
