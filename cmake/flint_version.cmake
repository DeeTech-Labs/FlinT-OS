include("${CMAKE_CURRENT_LIST_DIR}/../config/version.cmake")
set(PROJECT_VER "${FLINT_OS_VERSION_MAJOR}.${FLINT_OS_VERSION_MINOR}.${FLINT_OS_VERSION_PATCH}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/config")
configure_file("${CMAKE_CURRENT_LIST_DIR}/templates/flint_version.h.in"
    "${CMAKE_BINARY_DIR}/generated/config/flint_version.h" @ONLY)
