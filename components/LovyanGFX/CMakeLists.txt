# Slim LovyanGFX for FlinT OS (sprite + u8g2 fonts; extra sources from board/).
# Upstream sources live in third_party/LovyanGFX (cloned on first configure).

get_filename_component(FLINT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LGFX_ROOT "${FLINT_ROOT}/third_party/LovyanGFX")

# board_build.cmake is produced by tools/gen_board_catalog.py (root CMakeLists).
set(FLINT_BOARD_BUILD "${FLINT_ROOT}/components/flint/board/board_build.cmake")
if(EXISTS "${FLINT_BOARD_BUILD}")
    include("${FLINT_BOARD_BUILD}")
    message(STATUS "LovyanGFX board bus=${FLINT_BOARD_DISPLAY_BUS} impl=${FLINT_BOARD_DISPLAY_IMPL}")
else()
    message(FATAL_ERROR "Missing ${FLINT_BOARD_BUILD} — run tools/gen_board_catalog.py or reconfigure")
endif()

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
    ${LGFX_ROOT}/src/lgfx/v1/platforms/esp32/Light_PWM.cpp
)

# Bus / panel extras come from periph JSON via board_build.cmake — no per-board
# edits in this file.
if(NOT DEFINED FLINT_BOARD_LGFX_SOURCES OR FLINT_BOARD_LGFX_SOURCES STREQUAL "")
    message(FATAL_ERROR "FLINT_BOARD_LGFX_SOURCES empty — regenerate board_build.cmake")
endif()
foreach(_flint_lgfx_rel IN LISTS FLINT_BOARD_LGFX_SOURCES)
    list(APPEND LGFX_SRCS "${LGFX_ROOT}/src/lgfx/v1/${_flint_lgfx_rel}")
endforeach()

# Base IDF deps always present; periph may add more (spi, esp_lcd, …).
# esp_driver_i2c stays: platforms/esp32/common.cpp includes I2C headers unconditionally.
set(LGFX_REQUIRES
    efuse
    esp_timer
    esp_mm
    esp_driver_gpio
    esp_driver_ledc
    esp_driver_i2c
    freertos
    log
)
if(DEFINED FLINT_BOARD_IDF_REQUIRES AND NOT FLINT_BOARD_IDF_REQUIRES STREQUAL "")
    list(APPEND LGFX_REQUIRES ${FLINT_BOARD_IDF_REQUIRES})
endif()
list(REMOVE_DUPLICATES LGFX_REQUIRES)

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
