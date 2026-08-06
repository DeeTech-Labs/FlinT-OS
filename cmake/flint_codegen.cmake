# FlinT codegen — run Python tools only when inputs change (Ninja/Make dep graph).
# Included from the root CMakeLists.txt after project().

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(FLINT_TOOLS_DIR "${CMAKE_SOURCE_DIR}/tools")
set(FLINT_I18N_OUT "${CMAKE_SOURCE_DIR}/components/flint/i18n/i18n_catalog.inc")
set(FLINT_ASSET_PACK "${CMAKE_SOURCE_DIR}/assets/fs_root/core.flintpack")
set(FLINT_RESOURCES_H "${CMAKE_SOURCE_DIR}/components/flint/sdk/resources.h")
set(FLINT_VERSION_H "${CMAKE_SOURCE_DIR}/components/flint/config/flint_version.h")

# —— i18n catalog (→ firmware) ——
file(GLOB_RECURSE FLINT_I18N_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/components/flint/i18n/lang/*.pot"
    "${CMAKE_SOURCE_DIR}/components/flint/i18n/lang/*.po"
    "${CMAKE_SOURCE_DIR}/components/flint/i18n/lang/*.json"
)
add_custom_command(
    OUTPUT "${FLINT_I18N_OUT}"
    COMMAND ${Python3_EXECUTABLE} "${FLINT_TOOLS_DIR}/gen_i18n_catalog.py"
    DEPENDS
        "${FLINT_TOOLS_DIR}/gen_i18n_catalog.py"
        ${FLINT_I18N_SOURCES}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "FlinT: gen_i18n_catalog.py (lang sources changed)"
    VERBATIM
)
add_custom_target(flint_gen_i18n DEPENDS "${FLINT_I18N_OUT}")

idf_component_get_property(_flint_lib flint COMPONENT_LIB)
if(_flint_lib)
    add_dependencies(${_flint_lib} flint_gen_i18n)
endif()

# —— Asset pack (→ LittleFS storage image) ——
# Mirror tools/gen_asset_pack.py: assets/<category>/*.png, skip fs_root.
set(FLINT_ASSET_PNGS "")
file(GLOB FLINT_ASSET_DIRS CONFIGURE_DEPENDS LIST_DIRECTORIES true
    "${CMAKE_SOURCE_DIR}/assets/*")
foreach(_dir IN LISTS FLINT_ASSET_DIRS)
    if(IS_DIRECTORY "${_dir}")
        get_filename_component(_name "${_dir}" NAME)
        if(NOT _name STREQUAL "fs_root" AND NOT _name MATCHES "^\\.")
            file(GLOB _pngs CONFIGURE_DEPENDS "${_dir}/*.png")
            list(APPEND FLINT_ASSET_PNGS ${_pngs})
        endif()
    endif()
endforeach()

add_custom_command(
    OUTPUT "${FLINT_ASSET_PACK}"
    COMMAND ${Python3_EXECUTABLE} "${FLINT_TOOLS_DIR}/gen_asset_pack.py"
    DEPENDS
        "${FLINT_TOOLS_DIR}/gen_asset_pack.py"
        "${FLINT_RESOURCES_H}"
        "${FLINT_VERSION_H}"
        ${FLINT_ASSET_PNGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "FlinT: gen_asset_pack.py (PNG / FlintIconId / version changed)"
    VERBATIM
)
add_custom_target(flint_gen_asset_pack DEPENDS "${FLINT_ASSET_PACK}")

set(FLINT_ASSETS_FS_ROOT "${CMAKE_SOURCE_DIR}/assets/fs_root")
littlefs_create_partition_image(storage "${FLINT_ASSETS_FS_ROOT}" FLASH_IN_PROJECT
    DEPENDS flint_gen_asset_pack)
