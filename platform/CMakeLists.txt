cmake_minimum_required(VERSION 3.21)
project(FlintPlatform VERSION 1.0 LANGUAGES CXX)

add_library(flint_platform STATIC
    src/manifest.cpp
    src/storage.cpp
    src/resources.cpp
    src/runtime.cpp
    src/core.cpp
    src/fonts.cpp
)
target_include_directories(flint_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(flint_platform PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(flint_platform PUBLIC /utf-8)
    target_compile_definitions(flint_platform PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(flint_platform PRIVATE -Wall -Wextra)
endif()

option(FLINT_PLATFORM_TESTS "Build local platform tests" ON)
if(FLINT_PLATFORM_TESTS)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_executable(core_tests tests/core.cpp)
    target_link_libraries(core_tests PRIVATE flint_platform)
    add_test(NAME core_integration
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_core.py $<TARGET_FILE:core_tests>
    )
    add_executable(platform_tests tests/contracts.cpp)
    target_link_libraries(platform_tests PRIVATE flint_platform)
    add_test(NAME contracts COMMAND platform_tests)
    add_test(NAME package_interop
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_builder.py $<TARGET_FILE:platform_tests>
    )
endif()
