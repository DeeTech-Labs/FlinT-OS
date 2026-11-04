# FlinT OS — ESP-IDF project
cmake_minimum_required(VERSION 3.16)

# Build defaults live under config/ (not the project root).
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/config/sdkconfig.defaults")

# Firmware version from components/flint/config/flint_version.h (single edit point).
set(_flint_version_h "${CMAKE_CURRENT_LIST_DIR}/components/flint/config/flint_version.h")
file(STRINGS "${_flint_version_h}" _flint_ver_maj REGEX "^#define FLINT_OS_VERSION_MAJOR ")
file(STRINGS "${_flint_version_h}" _flint_ver_min REGEX "^#define FLINT_OS_VERSION_MINOR ")
file(STRINGS "${_flint_version_h}" _flint_ver_pat REGEX "^#define FLINT_OS_VERSION_PATCH ")
string(REGEX REPLACE "^#define FLINT_OS_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" FLINT_OS_VERSION_MAJOR "${_flint_ver_maj}")
string(REGEX REPLACE "^#define FLINT_OS_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" FLINT_OS_VERSION_MINOR "${_flint_ver_min}")
string(REGEX REPLACE "^#define FLINT_OS_VERSION_PATCH[ \t]+([0-9]+).*" "\\1" FLINT_OS_VERSION_PATCH "${_flint_ver_pat}")
set(PROJECT_VER "${FLINT_OS_VERSION_MAJOR}.${FLINT_OS_VERSION_MINOR}.${FLINT_OS_VERSION_PATCH}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build only main + transitive REQUIRES (skips bt/eth/fatfs/i2s/… unless pulled).
# Must be set after including project.cmake, before project().
idf_build_set_property(MINIMAL_BUILD ON)

# Device revision must exist before components/LovyanGFX and flint configure.
set(FLINT_DEVICE "" CACHE STRING "Supported FlinT product revision (default: Spark S3)")
# Reconfigure when the device registry or maintained hardware files change.
file(GLOB_RECURSE _flint_device_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/components/flint/board/models/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tools/select_device.py" ${_flint_device_sources})
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_flint_device_args "${CMAKE_CURRENT_LIST_DIR}/tools/select_device.py")
if(FLINT_DEVICE)
    list(APPEND _flint_device_args --device "${FLINT_DEVICE}")
endif()
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${_flint_device_args}
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    RESULT_VARIABLE FLINT_DEVICE_GEN_RESULT
    OUTPUT_VARIABLE FLINT_DEVICE_GEN_OUT
    ERROR_VARIABLE FLINT_DEVICE_GEN_ERR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT FLINT_DEVICE_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "select_device.py failed:\n${FLINT_DEVICE_GEN_ERR}\n${FLINT_DEVICE_GEN_OUT}")
endif()
message(STATUS "FlinT OS device: ${FLINT_DEVICE_GEN_OUT}")
message(STATUS "FlinT OS version: ${PROJECT_VER}")

project(flint_os)

# i18n + asset pack: regenerate only when inputs change (see cmake/flint_codegen.cmake).
# Device selection stays configure-time (needed before component CMake).
include("${CMAKE_CURRENT_LIST_DIR}/cmake/flint_codegen.cmake")
