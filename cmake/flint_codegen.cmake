find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB_RECURSE FLINT_PLATFORM_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/platform/system/*.json"
    "${CMAKE_SOURCE_DIR}/platform/examples/*/project.json"
    "${CMAKE_SOURCE_DIR}/platform/spec/*.json"
    "${CMAKE_SOURCE_DIR}/platform/tools/*.py"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FLINT_PLATFORM_INPUTS})
foreach(generator generate_registry.py build_system.py)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/platform/tools/${generator}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Platform generation failed: ${generator}")
    endif()
endforeach()
littlefs_create_partition_image(factory "${CMAKE_SOURCE_DIR}/platform/factory" FLASH_IN_PROJECT)
