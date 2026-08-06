# FlinT OS — ESP-IDF project
cmake_minimum_required(VERSION 3.16)

# Build defaults live under config/ (not the project root).
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/config/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build only main + transitive REQUIRES (skips bt/eth/fatfs/spiffs/i2s/… unless pulled).
# Must be set after including project.cmake, before project().
idf_build_set_property(MINIMAL_BUILD ON)

# Board profile must exist before components/LovyanGFX and flint configure.
set(FLINT_BOARD "" CACHE STRING "FlinT board id (components/flint/board/board_index.json)")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_flint_board_args "${CMAKE_CURRENT_LIST_DIR}/tools/gen_board_catalog.py")
if(FLINT_BOARD)
    list(APPEND _flint_board_args --board "${FLINT_BOARD}")
endif()
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${_flint_board_args}
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    RESULT_VARIABLE FLINT_BOARD_GEN_RESULT
    OUTPUT_VARIABLE FLINT_BOARD_GEN_OUT
    ERROR_VARIABLE FLINT_BOARD_GEN_ERR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT FLINT_BOARD_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "gen_board_catalog.py failed:\n${FLINT_BOARD_GEN_ERR}\n${FLINT_BOARD_GEN_OUT}")
endif()
message(STATUS "FlinT board: ${FLINT_BOARD_GEN_OUT}")

project(flint_os)
