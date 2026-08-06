#pragma once

// =============================================================================
// FlinT OS versions (edit here only)
// Firmware / Face API / asset pack — not the FlinT product marketing version.
// Pre-release: keep API / pack / face at 1 until the first public release.
// =============================================================================

// FlinT OS firmware semver (drives ESP-IDF PROJECT_VER from CMake).
#define FLINT_OS_VERSION_MAJOR 0
#define FLINT_OS_VERSION_MINOR 1
#define FLINT_OS_VERSION_PATCH 0
#define FLINT_OS_VERSION_STRING "0.1.0"

// Face SDK / FaceContext ABI — bump when community faces must recompile.
#define FLINT_OS_API_VERSION 1

// LittleFS core.flintpack header (tools/gen_asset_pack.py reads these).
#define FLINT_PACK_FORMAT_VER 1  // on-disk FLPK layout
#define FLINT_PACK_API_MIN 1     // minimum FLINT_OS_API_VERSION to load pack
#define FLINT_PACK_VER 1         // content revision (icons set)

// Built-in digital face (FaceInfo.version / FaceInfo.minApi).
#define FLINT_FACE_DIGITAL_VERSION 1
#define FLINT_FACE_DIGITAL_MIN_API 1
