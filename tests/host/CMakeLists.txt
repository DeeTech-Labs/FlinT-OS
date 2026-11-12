cmake_minimum_required(VERSION 3.21)
project(FlintOSHostTests LANGUAGES C CXX)
enable_testing()
set(FLINT_PLATFORM_TESTS OFF CACHE BOOL "" FORCE)
set(FLINT_SDK_TOOLS OFF CACHE BOOL "" FORCE)
set(FLINT_PLATFORM_DIR "${CMAKE_CURRENT_LIST_DIR}/../../platform" CACHE PATH "IgniteSDK checkout (defaults to the pinned submodule)")
add_subdirectory("${FLINT_PLATFORM_DIR}" platform)
file(GLOB_RECURSE _flint_host_device_settings CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../../config/devices/flint_spark/device.toml"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/modules/*/module.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/select_device.py" ${_flint_host_device_settings})
find_package(Python3 3.11 COMPONENTS Interpreter REQUIRED)
execute_process(COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/../../tools/select_device.py"
    --device flint_spark --build-dir "${CMAKE_CURRENT_BINARY_DIR}" --quiet COMMAND_ERROR_IS_FATAL ANY)
add_executable(display_tests display.cpp
    ../../src/modules/st7789/st7789.cpp
    ../../src/modules/st7789/display.cpp)
target_include_directories(display_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" idf ../../src ../../config)
target_link_libraries(display_tests PRIVATE Flint::Platform)
if(MSVC)
    target_compile_options(display_tests PRIVATE /W4)
else()
    target_compile_options(display_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME st7789_display COMMAND display_tests)
add_executable(usb_tests usb.cpp ../../src/usb/usb_link.cpp)
target_compile_definitions(usb_tests PRIVATE FLINT_USB_TEST_CLOCK)
target_include_directories(usb_tests PRIVATE idf ../../src)
target_link_libraries(usb_tests PRIVATE Flint::Platform)
add_test(NAME usb_transport COMMAND usb_tests)

add_executable(ble_address_tests ble_address.cpp)
target_include_directories(ble_address_tests PRIVATE ../../src)
target_compile_features(ble_address_tests PRIVATE cxx_std_20)
add_test(NAME ble_peer_address COMMAND ble_address_tests)

add_executable(touch_tests touch.cpp ../../src/input/touch_state.cpp)
target_include_directories(touch_tests PRIVATE ../../src)
target_link_libraries(touch_tests PRIVATE Flint::Platform)
add_test(NAME touch_input COMMAND touch_tests)

add_executable(cst816_tests cst816.cpp ../../src/modules/cst816/cst816.cpp)
target_include_directories(cst816_tests PRIVATE cst816_idf ../../src)
target_compile_features(cst816_tests PRIVATE cxx_std_20)
add_test(NAME cst816_driver COMMAND cst816_tests)

# Compile absent-device adapters without any peripheral driver headers/dependencies.
add_library(disabled_device_adapters OBJECT ../../src/display/disabled.cpp ../../src/input/disabled.cpp)
target_include_directories(disabled_device_adapters PRIVATE idf ../../src)
target_link_libraries(disabled_device_adapters PRIVATE Flint::Platform)

add_executable(peripherals_tests peripherals.cpp ../../src/hardware/i2c.cpp
    ../../src/modules/pcf85063/pcf85063.cpp ../../src/modules/qmi8658/qmi8658.cpp)
target_include_directories(peripherals_tests PRIVATE peripherals_idf cst816_idf ../../src)
target_compile_features(peripherals_tests PRIVATE cxx_std_20)
add_test(NAME i2c_peripherals COMMAND peripherals_tests)

add_executable(peripheral_output_tests peripheral_outputs.cpp ../../src/hardware/pwm.cpp
    ../../src/modules/battery_adc/battery_adc.cpp ../../src/modules/pwm_buzzer/pwm_buzzer.cpp
    ../../src/modules/pwm_backlight/pwm_backlight.cpp ../../src/modules/power_button/power_button.cpp)
target_include_directories(peripheral_output_tests PRIVATE peripherals_idf cst816_idf ../../src)
target_compile_features(peripheral_output_tests PRIVATE cxx_std_20)
add_test(NAME peripheral_outputs COMMAND peripheral_output_tests)

add_executable(environmental_tests environmental_sensors.cpp ../../src/hardware/i2c.cpp
    ../../src/hardware/bosch280.cpp ../../src/modules/veml7700/veml7700.cpp)
target_include_directories(environmental_tests PRIVATE peripherals_idf cst816_idf ../../src)
target_compile_features(environmental_tests PRIVATE cxx_std_20)
add_test(NAME environmental_sensors COMMAND environmental_tests)
