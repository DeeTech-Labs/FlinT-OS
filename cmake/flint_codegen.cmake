include("${FLINT_PLATFORM_DIR}/cmake/FlintGenerate.cmake")
flint_generate_sdk(OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/sdk")
# Never package source staging files or developer scratch files.
file(GLOB factory_contents "${FLINT_FACTORY_DIR}/*")
foreach(factory_file IN LISTS factory_contents)
    if(IS_DIRECTORY "${factory_file}" OR NOT factory_file MATCHES "\\.ignit$")
        message(FATAL_ERROR "Unexpected file in generated factory directory: ${factory_file}")
    endif()
endforeach()
