# Keep SPI validation/error returns and IRAM handlers; omit detailed check-macro
# messages to reduce IRAM code. FlinT still logs propagated errors. Scope this
# policy to SPI; assertions and other ESP-IDF components keep their diagnostics.
idf_component_get_property(_flint_spi_lib esp_driver_spi COMPONENT_LIB)
target_compile_definitions(${_flint_spi_lib} PRIVATE CONFIG_COMPILER_OPTIMIZATION_CHECKS_SILENT=1)
