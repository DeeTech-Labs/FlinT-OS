set(FLINT_PLATFORM_DIR "${CMAKE_CURRENT_LIST_DIR}/../platform" CACHE PATH "IgniteSDK checkout (defaults to the pinned submodule)")
if(NOT EXISTS "${FLINT_PLATFORM_DIR}/include/flint/contracts.h")
    message(FATAL_ERROR "Initialize IgniteSDK: git submodule update --init --recursive")
endif()
