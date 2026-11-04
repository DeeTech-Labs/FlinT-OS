# Architecture

The maintained architecture specification lives in [FlinT Platform](../platform/README.md). The `platform/` directory is a pinned Git submodule of Flint IgniteSDK, shared with Flare. Initialize it with `git submodule update --init --recursive`. Shared implementation changes belong in that repository.

Hardware: `components/flint/board/`, `display/`, `ble/` and `core/system.cpp`.
Portable services: `platform/src/` and `platform/include/flint/`.
Factory applications: `platform/system/` compiled into `platform/factory/`.
Development contracts: `platform/spec/` and generated registries.
