# FlinT Platform roadmap

The November 2026 architecture replaces native face registration and fixed-slot packages with a shared package platform. The target is ESP32-S3 Super Mini; C3 support is removed. All public contract versions remain 1 during development.

## Architecture and clean Core

Shared contracts, scoped consent, indexed binary packages, a protected factory volume and independent recovery form the base. Hardware and transport implementations remain separate from application content.

## Ignite Foundation and system applications

The package manager, resource manager, declarative Runtime, renderer and event scheduling serve both factory and user packages. Clock, Launcher, Settings, Theme and Locale are compiled packages. Installation, resume, updates and rollback are verified locally.

## Flare and Companion

Preserve the QML design system while moving application behavior to the new device manager, Hub, Studio, Preview and Companion services. Flare and the simulator share the OS platform source and binary contracts.

## Executable applications

Lua is the only application language. Its bounded runtime is enabled for the validated Spark profile; repeat resource, latency and failure-recovery measurements for other boards or new native APIs. Background services remain unsupported.

## Hardware validation

Once the board is connected, establish Flash/PSRAM capacity, validate the display wiring, USB/BLE behavior, memory headroom, interrupted-write behavior and power consumption. Host tests and successful compilation do not establish battery life or physical reliability.
