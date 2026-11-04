# Architecture

The maintained architecture specification lives in [FlinT Platform](../platform/README.md). This shared implementation is exported into Desktop; the firmware does not maintain a second package parser or renderer.

Hardware: `components/flint/board/`, `display/`, `ble/` and `core/system.cpp`.
Portable services: `platform/src/` and `platform/include/flint/`.
Factory applications: `platform/system/` compiled into `platform/factory/`.
Development contracts: `platform/spec/` and generated registries.
