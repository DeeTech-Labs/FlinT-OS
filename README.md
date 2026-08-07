<p align="center">
  <img src="brand/FlinT_logo.png" alt="FlinT logo" width="280"/>
</p>

<h1 align="center">FlinT OS</h1>

<p align="center">
  <strong>Open firmware platform</strong> for FlinT devices · pluggable faces · by
  <a href="https://github.com/deetech-labs">DeeTech Labs</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-alpha-yellow?style=flat-square" alt="Status"/>
  <img src="https://img.shields.io/badge/platform-ESP32--C3-blue?style=flat-square" alt="ESP32-C3"/>
  <img src="https://img.shields.io/badge/framework-ESP--IDF-red?style=flat-square" alt="ESP-IDF"/>
  <img src="https://img.shields.io/badge/display-ST7789%20240×280-orange?style=flat-square" alt="ST7789"/>
  <img src="https://img.shields.io/badge/faces-pluggable-brightgreen?style=flat-square" alt="Faces"/>
  <img src="https://img.shields.io/badge/org-DeeTech%20Labs-111111?style=flat-square" alt="DeeTech Labs"/>
  <img src="https://img.shields.io/badge/license-Apache%202.0-blue?style=flat-square" alt="Apache 2.0"/>
</p>

---

## What it is

**FlinT OS** is the firmware for **FlinT** devices, developed by **DeeTech Labs**.
Core services stay stable; **faces are plugins** — add a theme without rewriting
networking or time.

| Built-in today | Coming next |
|----------------|-------------|
| Digital face + day modes + glances | More community faces |
| Soft-clock + SNTP + timezone DB | SoftAP provisioning |
| Wi‑Fi, settings (`cfg`), face picker | Hardware buttons (serial keys for now) |
| Event bus | OTA flow |
| Boot splash | |

Architecture: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) · Face SDK: `components/flint/sdk/flint_face_sdk.h`

---

## Quick start (ESP-IDF)

1. **Board:** FlinT Spark (ESP32-C3, USB Serial/JTAG)
2. **ESP-IDF** v5.x / v6.x activated (`source …/activate_idf_….sh` or EIM)
3. **Secrets:**
   ```bash
   cp components/flint/config/secrets.example.h components/flint/config/secrets.h
   ```
   Edit Wi‑Fi credentials (`secrets.h` is gitignored).
4. **Build & flash:**
   ```bash
   idf.py set-target esp32c3
   idf.py build
   idf.py -p /dev/ttyACM0 flash monitor
   ```
   Optional board profile (default **FlinT Spark** / `flint_spark`):
   ```bash
   idf.py -DFLINT_BOARD=flint_spark build
   # or: python3 tools/gen_board_catalog.py --board flint_spark --build
   ```
   See [`components/flint/board/README.md`](components/flint/board/README.md) (full setup guide).

### Serial controls (no buttons yet)

In the IDF monitor, type keys (no Enter needed except where noted):

| Key | Action |
|-----|--------|
| `Enter` | Face picker (from face) / apply face / open Face row |
| `s` | Settings |
| `p` / `n` | Navigate (picker item or settings row) |
| `+` / `-` | Adjust selected setting only |
| `q` / `Esc` | Back to face |
| `h` | Help in log |

Active face and prefs are stored on the `cfg` LittleFS partition (face, seconds, hour format, language, timezone, soft clock). Clock time/date can be adjusted in Settings (`+/-` = ±1 minute / ±1 day) and persist until the next NTP apply.

Settings: `p`/`n` navigate, `+/-` change value, `Enter` only opens face picker from the Face row. Shell transitions are instant (no fade wash).

Dependencies:
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) — slim local component (`components/LovyanGFX`); `src/` sparse-cloned to `third_party/LovyanGFX/` (no upstream examples)

Flash is kept lean: no LVGL, no C++ exceptions, STA-only WPA2 Wi‑Fi (no SoftAP/WPA3), trimmed LWIP/mbedTLS (no HTTP client). See `config/sdkconfig.defaults`.

---

## Project layout

```
flint_os/
├── main/                 # entry only (app_main)
├── config/               # sdkconfig.defaults + partitions.csv
├── components/
│   ├── flint/            # ★ all product code
│   │   ├── config/       # flint_version.h (FlinT OS API/firmware), secrets.h (local)
│   │   ├── board/        # ★ hardware profiles (JSON → board_config.inc)
│   │   ├── app/ shell/ settings/ sdk/
│   │   ├── faces/        # Face API + themes
│   │   └── display/ net/ time/ …
│   └── LovyanGFX/        # slim CMake (bus sources from board)
├── tools/                # codegen — see tools/README.md (or: ./flint_tools)
├── assets/               # icons/ splash/ chrome/ glances/ + fs_root pack
│   └── README.md
├── flint_tools           # bash picker for tools/*.py
├── third_party/LovyanGFX/# upstream sources (gitignored, cloned on build)
├── community/            # face templates
└── brand/                # marketing logo (README), not device assets
```

Icons load from the `storage` LittleFS partition when flashed; see [`assets/README.md`](assets/README.md). UI prefs and soft clock live on the separate `cfg` LittleFS partition (runtime R/W; survives asset reflash).

---

## Add a face

1. Copy `community/face_template.cpp.example` → `components/flint/faces/face_yourname.cpp`
2. `#include "sdk/flint_face_sdk.h"` only — implement `Face`, export `Face* createYourFace()`
3. Register in `components/flint/faces/faces_builtin.cpp`
4. Build & flash; pick via serial face picker (or default `ACTIVE_FACE_ID` on empty `cfg`)

---

## Hardware (base device)

**FlinT Spark** (`flint_spark` in `components/flint/board/`) — default / reference
board for FlinT OS.

| Signal | ESP32-C3 |
|--------|----------|
| SCK | GPIO 6 |
| MOSI (SDA) | GPIO 7 |
| CS | GPIO 10 |
| DC | GPIO 9 |
| RST | GPIO 8 |
| VCC / LED | 3.3V |
| GND | GND |

Display: GMT028-class panel, **ST7789** 240×280 SPI (landscape UI 280×240).

To change pins or panel: edit JSON under `components/flint/board/`
(and rebuild with `-DFLINT_BOARD=…`). Common SPI panels need no C++ — see the board guide.

---

## License

Copyright 2026 DeeTech Labs

Licensed under the [Apache License, Version 2.0](LICENSE).

You may use, modify, and distribute this software. Redistributions must keep the
copyright notice, license text, and attribution from [`NOTICE`](NOTICE)
(original work by DeeTech Labs for the FlinT product / FlinT OS firmware).

---

## Contributing

- Prefer **new files under `components/flint/faces/`** over edits to services
- Include only `sdk/flint_face_sdk.h` from faces; keep `onTick` non-blocking
- Hardware variants: see guide in [`components/flint/board/README.md`](components/flint/board/README.md)
- Never commit `components/flint/config/secrets.h`

---

<p align="center">
  <sub>FlinT OS · DeeTech Labs · Apache 2.0 · ESP-IDF</sub>
</p>
