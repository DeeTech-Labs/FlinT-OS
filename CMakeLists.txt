# FlinT OS — ESP-IDF project
cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/flint_platform.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/flint_version.cmake")

set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/config/sdkconfig.defaults")
# Persistent local configuration survives fullclean; an explicit -DSDKCONFIG wins.
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_CURRENT_LIST_DIR}/config/sdkconfig")
endif()
get_filename_component(SDKCONFIG "${SDKCONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_flint_sdkconfig_dir "${SDKCONFIG}" DIRECTORY)
file(MAKE_DIRECTORY "${_flint_sdkconfig_dir}")
if(SDKCONFIG STREQUAL "${CMAKE_CURRENT_LIST_DIR}/config/sdkconfig"
        AND NOT EXISTS "${SDKCONFIG}" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig"
        AND NOT "$ENV{_IDF_PY_SET_TARGET_ACTION}" STREQUAL "1")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/sdkconfig" "${SDKCONFIG}" COPYONLY)
    message(STATUS "Imported legacy root sdkconfig into ${SDKCONFIG}")
endif()
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/src")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_BINARY_DIR}/dependencies.lock")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/flint_device.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/flint_codegen.cmake")
project(flint_os)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/flint_firmware.cmake")
littlefs_create_partition_image(factory "${FLINT_FACTORY_DIR}" FLASH_IN_PROJECT)
