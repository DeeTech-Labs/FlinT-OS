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
  <img src="https://img.shields.io/badge/display-ST7789%20240×320-orange?style=flat-square" alt="ST7789"/>
  <img src="https://img.shields.io/badge/faces-pluggable-brightgreen?style=flat-square" alt="Faces"/>
  <img src="https://img.shields.io/badge/Arduino-compatible-00979D?style=flat-square&logo=arduino&logoColor=white" alt="Arduino"/>
  <img src="https://img.shields.io/badge/org-DeeTech%20Labs-111111?style=flat-square" alt="DeeTech Labs"/>
  <img src="https://img.shields.io/badge/license-Apache%202.0-blue?style=flat-square" alt="Apache 2.0"/>
</p>

---

## What it is

**FlinT OS** is the software stack for the FlinT ecosystem. Core services stay stable; **faces are plugins** — add a theme without rewriting networking, time, or weather.

| Built-in today | Coming next |
|----------------|-------------|
| Digital face + day modes (morning / day / evening / night) | More community faces |
| Soft-clock + NTP | NVS settings / face picker |
| Open-Meteo weather (async) | SoftAP provisioning |
| Wi‑Fi, modular `src/` layout | Power / input abstractions |

---

## Quick start

1. **Board:** ESP32C3 Dev Module (USB CDC On Boot: Enabled)
2. **Libraries:** Adafruit GFX · Adafruit ST7735 and ST7789 · Adafruit BusIO · U8g2 · U8g2_for_Adafruit_GFX
3. **Secrets:**
   ```bash
   cp src/config/secrets.example.h src/config/secrets.h
   ```
   Edit `src/config/secrets.h` with your Wi‑Fi credentials (`secrets.h` is gitignored).
4. Open the sketch folder in Arduino IDE (folder name must be `flint_os`) and flash `flint_os.ino`.

> Includes are sketch-root relative, e.g. `#include "src/app/app.h"`.

---

## Project layout

```
FlinT OS /
├── flint_os.ino           # entry (setup / loop)
├── brand/                 # logo & identity
├── community/             # face templates for contributors
└── src/
    ├── app/               # orchestration
    ├── config/            # pins, defaults, secrets (local)
    ├── display/           # TFT / U8g2 HAL
    ├── day_mode/          # time-of-day profiles
    ├── time/              # soft-clock + NTP
    ├── net/               # Wi‑Fi
    ├── weather/           # Open-Meteo (background task)
    ├── ui/                # active-face shell
    └── faces/             # ← drop your face here
```

---

## Add a face

1. Copy `community/face_template.cpp.example` → `src/faces/face_yourname.cpp`
2. Implement `Face`, register with `FLINT_REGISTER_FACE(...)`
3. Set in `src/config/config.h`:
   ```cpp
   static const char* ACTIVE_FACE_ID = "yourname";
   ```
4. Build & flash — **no core edits required**

---

## Hardware (reference)

| Signal | ESP32-C3 |
|--------|----------|
| SCK | GPIO 6 |
| MOSI (SDA) | GPIO 7 |
| CS | GPIO 10 |
| DC | GPIO 9 |
| RST | GPIO 8 |
| VCC / LED | 3.3V |
| GND | GND |

Display: GMT028-05 class panel, **ST7789** 240×320 SPI.

---

## License

Copyright 2026 DeeTech Labs

Licensed under the [Apache License, Version 2.0](LICENSE).

You may use, modify, and distribute this software. Redistributions must keep the
copyright notice, license text, and attribution from [`NOTICE`](NOTICE)
(original work by DeeTech Labs / FlinT OS).

---

## Contributing

FlinT OS is a small community platform: pluggable faces, stable core.

- Prefer **new files under `src/faces/`** over edits to services
- Keep `onTick` non-blocking (no HTTP / long `delay`)
- Never commit `src/config/secrets.h`

---

<p align="center">
  <sub>FlinT OS · DeeTech Labs · Apache 2.0</sub>
</p>
