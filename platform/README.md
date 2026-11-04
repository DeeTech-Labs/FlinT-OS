# FlinT Platform v1

The shared C++20 platform used by FlinT OS, Desktop Preview and the local simulator. Public OS API, Face SDK, Ignite format and Companion protocol versions remain **1**. Development contract revision and build identity are checked separately; incompatible binaries must never be accepted merely because both report v1.

The canonical source lives in the OS repository. `tools/sync_platform.py` exports this entire directory into the independently buildable Desktop repository. `--check` detects drift. Edit the canonical source, regenerate the factory packages and registries, then export. There is one implementation of each contract, renderer and package engine, distributed to both applications.

## Architecture

The ESP-IDF layer owns hardware, native USB, authenticated NimBLE, mounting and recovery. Portable Core owns package transactions, consent, time, component selection, event scheduling and application lifecycle. Runtime reads compiled UI and individual resources through indexed file access. Factory packages and user packages use the same manifest, validator and Runtime. Core selects factory components by role metadata; it contains no built-in watchface layouts or application-name switch statements.

Desktop preserves its QML shell, visual theme, icons, fonts, effects and localization. Its service layer consists of PlatformClient (device/transport lifecycle), IgniteHub (packages and consent), Studio (project compilation), Preview (shared renderer) and CompanionService (host policy and metrics). Its simulator runs the exact same portable Core through binary framed requests, in a disposable directory, without opening a device.

API and permission registries are generated from `spec/services.json` and `spec/permissions.json`. Services that are not implemented return `not_supported`. Listing a future permission does not advertise a working hardware or host integration.

## Ignite container

Integers are little endian, except compiled UI scenes, whose codec explicitly uses big endian. Never cast wire bytes to native C++ structs.

The 64-byte container header contains: magic `FLIGNIT\0` at 0; u16 format version at 8; u16 header size at 10; u32 development revision at 12; u32 total length at 16; u32 entry count at 20; u32 index offset at 24; u32 reserved zero at 28; SHA-256 at 32. The hash covers the entire file with the digest field zeroed.

Index entries are 24 bytes: u32 ID, u16 section type, u16 codec, u32 offset, u32 length, u32 flags, u32 reserved. IDs increase strictly, sections do not overlap and the final section ends at EOF. Exactly one section is a manifest. Supported sections are manifest=1, component=2, resource=3, UI=4, Wasm=5, locale=6, signature=7. Codec, flags and reserved must be zero. Package size is bounded to 1 MiB, section count to 1024 and manifest size to 4096 bytes. These are parser/security limits, not fixed installation slots.

The manifest starts with `FLM1`, followed by u16-length UTF-8 strings for ID/name/publisher; u32 package version; three u32 memory/instruction budgets; u64 required capabilities; u16 permission count with permission/scope string pairs; u16 component count with ID string, u16 type, u8 role and u32 section ID. Permission count is bounded to 64 and components to 32. IDs are restricted identifiers and can never contain path traversal. Unknown permissions are rejected.

Compiled UI supports rectangles, rounded rectangles, lines, text, small masks and indexed RGB565 images. Text bindings expose only declared APIs. Own resource access requires `ui.resources`. Fonts use indexed FLGF glyph atlases with bounded scratch storage; images use FLIM plus dimensions and RGB565 rows. The compiler performs TTF/PNG conversion on the computer. Nothing on the watch parses TTF, SVG, YAML or source JSON.

The scene description and built-in rasterizer/font algorithms were retained as portable graphics primitives. The old FaceRegistry, fixed layouts, IgnitDefinition, three-slot manager, textual installer and command parser were removed.

## Storage and recovery

Factory packages are read-only filesystem contents built with the firmware. User packages are immutable objects named by their container digest. Each user package has an atomic `.ref` and a previous-version reference. Installation uses `incoming.part`, durable block writes, a complete-file transfer digest, container/manifest/UI validation, then an atomic reference commit. Repeating a block is allowed only when its existing bytes match. Reopening the same transfer resumes at its durable offset.

An interrupted install cannot become active. Startup restores a valid previous reference when the current object is invalid. Failed active updates reload the previous version. User operations cannot replace or remove factory identities. Unreferenced objects and interrupted atomic-write temporaries are collected; the current transfer is retained for resume. The production catalog visits one manifest at a time instead of retaining every inactive manifest in RAM.

Recovery remains outside user packages. Desktop can restore factory role selections. The firmware can show its own recovery screen and process USB independently of the active Ignite. NVS trust keys and non-erased corrupt filesystems are never automatically wiped.

## Security

A/P/S/U/X/R grants bind application, claimed publisher, installed content generation, exact scope, expiry, session or explicit action. Automatic access still requires a manifest declaration. Restricted rights are unavailable to ordinary user packages. Session consent ends on disconnect; upgrades do not inherit expanded or earlier-version consent. Revocation invalidates samples and queued events.

CPU/RAM are available only with an active matching `companion.pc.metrics` grant scoped to `cpu,ram`, and the independent Desktop policy enabled. Samples received without watch consent are rejected. Samples expire after ten seconds, and switching apps clears them. The host collector currently supports Linux; unsupported collectors do not fabricate measurements.

SHA-256 verifies integrity, not publisher authenticity. User publisher strings are never treated as signatures or privileged identity. Factory trust comes from the protected firmware-provisioned volume. No user package has access to QProcess, arbitrary shell commands, raw GPIO, another app's memory or transport internals.

BLE commands require encryption, authentication and bonding. Numeric comparison is authorized through the retained local USB channel; BLE cannot open or approve its own pairing window. USB and BLE use the same framed service contract, without a textual fallback.

## Companion framing

A frame is `FLCP`, u8 version=1, u8 response flag, u16 method ID, u32 nonzero request ID, u32 payload length, payload, then CRC-32 over header and payload. Maximum payload is 4096 bytes. Readers accept fragmented frames, reject invalid checksums and discard leading noise. Responses start with a u16 Error code. Payloads use the shared explicit Writer/Reader codecs; strings carry u16 byte lengths. Service IDs are fixed by `spec/services.json` and checked against the C++ enum.

The handshake carries protocol, development revision, build ID and required capabilities. No state-changing operation is accepted before compatibility and transport trust checks. Transfer chunks are independent of ATT fragmentation. BLE uses acknowledged 20-byte GATT fragments and generation checks to discard disconnected work.

## Build and local validation

```sh
python3 -m pip install Pillow
python3 platform/tools/generate_registry.py
python3 platform/tools/build_system.py
cmake -S platform -B /tmp/flint-platform -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/flint-platform
ctest --test-dir /tmp/flint-platform --output-on-failure
python3 -m unittest discover -s tools/tests -v
python3 tools/sync_platform.py --check
```

Build an application without changing or flashing OS source:

```sh
python3 platform/tools/compile_ignite.py \
    platform/examples/pc-monitor/project.json /tmp/pc-monitor.ignit
```

Integration tests exercise resume, duplicate chunks, integrity failures, updates, rollback, factory protection, permission checks and revocation, custom fonts/images, stale handles, repeated switching and framing. Desktop's offline smoke test loads the actual QML interface and simulator. Physical device tests are excluded.

Use four spaces, no tabs, in maintained source. `.editorconfig` and `.clang-format` capture the project style. Generated font data is not manually reformatted. `tools/build_builtin_fonts.py --font-dir /path/to/lato` regenerates the common built-in font subsets.

## Explicit remaining limits

Wasm/WAMR and executable background services are not advertised or executed: their engine selection is conditional on measurements from the target board. Unsupported code sections return `not_supported`. The registry includes future API permissions without claiming that those sensors, media integrations or privileged actions exist. Current time settings use a UTC offset.

The supplied marking `esp32s3 112026` does not establish Flash or PSRAM capacity. The firmware uses a provisional 4 MiB layout with two OTA-sized application slots, a factory volume and writable storage. PSRAM is optional; the renderer prefers it when available and stages DMA rows in internal memory. Real memory headroom, wiring, BLE behavior, flash/power-loss behavior and battery life still require device validation. No battery-life or on-device reliability claim follows from host tests.

Design sources: [full discussion](https://chatgpt.com/share/6aea633e-6744-83eb-8ef1-564b610456bb), [final summary](https://chatgpt.com/share/6aeb20f7-8fbc-83ed-8fde-b505b0d2baf9). Later S3 decisions supersede earlier C3 and custom Logic Bytecode proposals.
