# Generate selected board headers before ESP-IDF scans component requirements.
set(FLINT_DEVICE "" CACHE STRING "Supported FlinT product revision (default: Spark S3)")
file(GLOB_RECURSE _flint_device_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../src/board/models/*"
    "${CMAKE_CURRENT_LIST_DIR}/../config/devices/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../tools/select_device.py"
    "${CMAKE_CURRENT_LIST_DIR}/../config/devices.json" ${_flint_device_sources})
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_flint_device_args "${CMAKE_CURRENT_LIST_DIR}/../tools/select_device.py"
    --build-dir "${CMAKE_BINARY_DIR}")
if(FLINT_DEVICE)
    list(APPEND _flint_device_args --device "${FLINT_DEVICE}")
endif()
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${_flint_device_args}
    RESULT_VARIABLE _flint_device_result
    OUTPUT_VARIABLE _flint_device_out
    ERROR_VARIABLE _flint_device_err
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
if(NOT _flint_device_result EQUAL 0)
    message(FATAL_ERROR "select_device.py failed:\n${_flint_device_err}\n${_flint_device_out}")
endif()
message(STATUS "FlinT OS device: ${_flint_device_out}")
message(STATUS "FlinT OS version: ${PROJECT_VER}")
