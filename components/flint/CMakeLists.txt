get_filename_component(FLINT_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
include("${FLINT_COMPONENT_DIR}/board/device.cmake")
if(NOT "${FLINT_DEVICE_IDF_TARGET}" STREQUAL "${IDF_TARGET}")
    message(FATAL_ERROR "FlinT requires esp32s3; run idf.py set-target esp32s3")
endif()
if(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    message(FATAL_ERROR "Binary Companion requires CONFIG_ESP_CONSOLE_NONE=y; use the current sdkconfig.defaults")
endif()
set(PLATFORM "${FLINT_COMPONENT_DIR}/../../platform")
idf_component_register(
    SRCS core/system.cpp display/display.cpp ble/ble_link.cpp
        "${PLATFORM}/src/manifest.cpp" "${PLATFORM}/src/storage.cpp" "${PLATFORM}/src/runtime.cpp"
        "${PLATFORM}/src/resources.cpp" "${PLATFORM}/src/core.cpp" "${PLATFORM}/src/fonts.cpp"
    INCLUDE_DIRS "." "${PLATFORM}/include"
    REQUIRES esp_app_format bt esp_partition nvs_flash esp_timer esp_driver_gpio freertos heap log esp_driver_usb_serial_jtag LovyanGFX joltwallet__littlefs
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)
