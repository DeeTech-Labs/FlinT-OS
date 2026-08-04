# Slim LovyanGFX for FlinT OS (ST7789 SPI + sprite + u8g2 fonts).
# Upstream sources live in third_party/LovyanGFX (cloned on first configure).

get_filename_component(FLINT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LGFX_ROOT "${FLINT_ROOT}/third_party/LovyanGFX")

if(NOT EXISTS "${LGFX_ROOT}/src/LovyanGFX.hpp")
    message(STATUS "Cloning LovyanGFX (src only) into ${LGFX_ROOT}")
    find_package(Git REQUIRED)
    file(MAKE_DIRECTORY "${FLINT_ROOT}/third_party")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone --depth 1 --filter=blob:none --sparse
                https://github.com/lovyan03/LovyanGFX.git "${LGFX_ROOT}"
        RESULT_VARIABLE LGFX_CLONE_RESULT
        ERROR_VARIABLE LGFX_CLONE_ERR
    )
    if(NOT LGFX_CLONE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clone LovyanGFX: ${LGFX_CLONE_ERR}")
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${LGFX_ROOT}" sparse-checkout set --no-cone
                /src /license.txt
        RESULT_VARIABLE LGFX_SPARSE_RESULT
        ERROR_VARIABLE LGFX_SPARSE_ERR
    )
    if(NOT LGFX_SPARSE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed LovyanGFX sparse-checkout: ${LGFX_SPARSE_ERR}")
    endif()
endif()

set(LGFX_SRCS
    ${LGFX_ROOT}/src/lgfx/v1/LGFXBase.cpp
    ${LGFX_ROOT}/src/lgfx/v1/LGFX_Sprite.cpp
    ${LGFX_ROOT}/src/lgfx/v1/LGFX_Button.cpp
    ${LGFX_ROOT}/src/lgfx/v1/lgfx_fonts.cpp
    ${LGFX_ROOT}/src/lgfx/v1/misc/common_function.cpp
    ${LGFX_ROOT}/src/lgfx/v1/misc/pixelcopy.cpp
    ${LGFX_ROOT}/src/lgfx/v1/misc/DividedFrameBuffer.cpp
    ${LGFX_ROOT}/src/lgfx/v1/misc/SpriteBuffer.cpp
    ${LGFX_ROOT}/src/lgfx/v1/panel/Panel_Device.cpp
    ${LGFX_ROOT}/src/lgfx/v1/panel/Panel_LCD.cpp
    ${LGFX_ROOT}/src/lgfx/v1/platforms/esp32/common.cpp
    ${LGFX_ROOT}/src/lgfx/v1/platforms/esp32/Bus_SPI.cpp
    ${LGFX_ROOT}/src/lgfx/v1/platforms/esp32/Light_PWM.cpp
)

# SPI bus + LEDC backlight only. No I2S parallel, RGB, or esp_lcd panel stack.
# esp_driver_i2c stays: platforms/esp32/common.cpp includes I2C headers unconditionally.
set(LGFX_REQUIRES
    efuse
    esp_timer
    esp_mm
    esp_driver_gpio
    esp_driver_spi
    esp_driver_ledc
    esp_driver_i2c
    freertos
    log
)

idf_component_register(
    SRCS ${LGFX_SRCS}
    INCLUDE_DIRS ${LGFX_ROOT}/src
    REQUIRES ${LGFX_REQUIRES}
)

# LGFXBase.cpp references JPG/PNG/QR helpers; codecs are omitted and unused
# methods are dropped by --gc-sections.
# Silence ESP-IDF LEDC deprecation in Light_PWM (upstream still sets intr_type).
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os -ffunction-sections -fdata-sections
    -Wno-deprecated-declarations
)
