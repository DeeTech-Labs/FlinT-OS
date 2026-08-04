# FlinT OS — ESP-IDF project (ESP32-C3)
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build only main + transitive REQUIRES (skips bt/eth/fatfs/spiffs/i2s/… unless pulled).
# Must be set after including project.cmake, before project().
idf_build_set_property(MINIMAL_BUILD ON)

project(flint_os)
