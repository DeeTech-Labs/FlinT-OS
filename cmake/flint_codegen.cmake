find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB_RECURSE FLINT_PLATFORM_INPUTS CONFIGURE_DEPENDS
    "${FLINT_PLATFORM_DIR}/system/*.json"
    "${FLINT_PLATFORM_DIR}/examples/*/project.json"
    "${FLINT_PLATFORM_DIR}/spec/*.json"
    "${FLINT_PLATFORM_DIR}/tools/*.py"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FLINT_PLATFORM_INPUTS})
foreach(generator generate_registry.py build_system.py)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${FLINT_PLATFORM_DIR}/tools/${generator}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Platform generation failed: ${generator}")
    endif()
endforeach()
littlefs_create_partition_image(factory "${FLINT_PLATFORM_DIR}/factory" FLASH_IN_PROJECT)
