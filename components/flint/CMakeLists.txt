get_filename_component(FLINT_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
include("${FLINT_COMPONENT_DIR}/board/device.cmake")
if(NOT "${FLINT_DEVICE_IDF_TARGET}" STREQUAL "${IDF_TARGET}")
    message(FATAL_ERROR "FlinT requires esp32s3; run idf.py set-target esp32s3")
endif()
if(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG OR CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG)
    message(FATAL_ERROR "Binary Companion owns USB Serial/JTAG: select CONFIG_ESP_CONSOLE_NONE=y and CONFIG_ESP_CONSOLE_SECONDARY_NONE=y (including existing sdkconfig)")
endif()
set(PLATFORM "${FLINT_PLATFORM_DIR}")
if(CONFIG_FLINT_LUA_RUNTIME)
    include("${PLATFORM}/cmake/LuaSources.cmake")
endif()
idf_component_register(
    SRCS ${FLINT_LUA_CORE} core/system.cpp display/display.cpp display/st7789.cpp ble/ble_link.cpp usb/usb_link.cpp
        "${PLATFORM}/src/lua_app.cpp" "${PLATFORM}/src/manifest.cpp" "${PLATFORM}/src/storage.cpp" "${PLATFORM}/src/runtime.cpp"
        "${PLATFORM}/src/resources.cpp" "${PLATFORM}/src/core.cpp" "${PLATFORM}/src/fonts.cpp"
    INCLUDE_DIRS "." "${PLATFORM}/include"
    LDFRAGMENTS usb/usb.lf
    REQUIRES esp_app_format bt esp_partition nvs_flash esp_timer esp_driver_gpio freertos heap log esp_driver_usb_serial_jtag joltwallet__littlefs ${FLINT_DEVICE_IDF_REQUIRES}
)
if(NOT CONFIG_ESP_TIMER_IN_IRAM)
    message(FATAL_ERROR "USB session observer runs during Flash writes: CONFIG_ESP_TIMER_IN_IRAM=y is required")
endif()

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)

if(CONFIG_FLINT_LUA_RUNTIME)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${FLINT_LUA_SOURCE_ROOT}")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ${FLINT_LUA_DEFINITIONS})
endif()
