<p align="center">
  <img src="brand/FlinT_logo.png" alt="FlinT" width="220"/>
</p>

<h1 align="center">FlinT OS</h1>

<p align="center">
  <b>Open firmware</b> for <b>FlinT</b> devices · pluggable faces · ESP32‑C3 / ESP‑IDF<br/>
  by <a href="https://github.com/DeeTech-Labs">DeeTech Labs</a>
</p>

<p align="center">
  <a href="https://github.com/DeeTech-Labs/FlinT-OS/stargazers"><img src="https://img.shields.io/github/stars/DeeTech-Labs/FlinT-OS?style=flat-square&logo=github&color=ff6a00&labelColor=111111" alt="Stars"/></a>
  <img src="https://img.shields.io/badge/status-alpha-ff6a00?style=flat-square&labelColor=111111" alt="Alpha"/>
  <img src="https://img.shields.io/badge/SoC-ESP32--C3-a8a59c?style=flat-square&labelColor=111111" alt="ESP32-C3"/>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache%202.0-e8e4dc?style=flat-square&labelColor=111111" alt="License"/></a>
</p>

<p align="center">
  <a href="#quick-start">Quick start</a> ·
  <a href="#docs">Docs</a> ·
  <a href="docs/FACES.md">Add a face</a> ·
  <a href="https://github.com/DeeTech-Labs/FlinT-OS/discussions">Discussions</a>
</p>

---

## What it is

**FlinT OS** is the firmware platform for **FlinT** devices (base board: **FlinT Spark**), by **DeeTech Labs**.

Core services stay stable — time, Wi‑Fi, settings, shell, assets. **Faces are plugins**: ship a new look without rewriting networking or the clock.

<table>
  <tr>
    <td width="50%" valign="top">
      <h3>Built-in</h3>
      <ul>
        <li>Digital face · day modes · glances</li>
        <li>Soft-clock + SNTP + timezone DB</li>
        <li>Wi‑Fi STA · settings on <code>cfg</code> LittleFS</li>
        <li>Face picker · event bus · boot splash</li>
      </ul>
    </td>
    <td width="50%" valign="top">
      <h3>Stack</h3>
      <ul>
        <li>ESP‑IDF · LovyanGFX (slim)</li>
        <li><code>-Os</code> · no C++ exceptions / RTTI</li>
        <li>Dual OTA · assets on <code>storage</code></li>
        <li>Input: USB serial (buttons later)</li>
      </ul>
    </td>
  </tr>
</table>

---

## Quick start

**Board:** FlinT Spark (ESP32‑C3, USB Serial/JTAG) · **ESP‑IDF** v5.x / v6.x

```bash
cp components/flint/config/secrets.example.h components/flint/config/secrets.h
# edit SSID / password (secrets.h is gitignored)

idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Optional: `idf.py -DFLINT_BOARD=flint_spark build` · board guide: [`components/flint/board/README.md`](components/flint/board/README.md)

<details>
<summary><b>Serial controls</b></summary>

Type in the IDF monitor (no Enter except where noted):

| Key | Action |
|:---:|--------|
| `Enter` | Face picker / apply face / open Face row |
| `s` | Settings |
| `p` / `n` | Navigate |
| `+` / `-` | Adjust selected setting |
| `q` / `Esc` | Back to face |
| `h` | Help in log |

Prefs and soft clock live on `cfg` LittleFS (survive asset reflash). Icons load from `storage` — [`assets/README.md`](assets/README.md).

</details>

<details>
<summary><b>Hardware — FlinT Spark pinout</b></summary>

Default board id: `flint_spark`. Display: GMT028-class **ST7789** 240×280 SPI (UI landscape 280×240).

| Signal | GPIO |
|--------|:----:|
| SCK | 6 |
| MOSI (SDA) | 7 |
| CS | 10 |
| DC | 9 |
| RST | 8 |
| VCC / LED | 3.3V |
| GND | GND |

Change pins/panel via JSON under `components/flint/board/`, then rebuild with `-DFLINT_BOARD=…`.

</details>

<details>
<summary><b>Project layout</b></summary>

```
flint_os/
├── main/                 # app_main only
├── config/               # sdkconfig.defaults + partitions.csv
├── components/
│   ├── flint/            # ★ product firmware
│   └── LovyanGFX/        # slim graphics component
├── tools/                # codegen (or ./flint_tools)
├── assets/               # PNG → LittleFS pack
├── community/            # face templates
└── brand/                # marketing logo
```

</details>

---

## Docs

| Doc | Contents |
|-----|----------|
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Dependencies, Face contract, dirty events |
| [`docs/MODULES.md`](docs/MODULES.md) | What each directory owns |
| [`docs/FACES.md`](docs/FACES.md) | Add / register a face |
| [`docs/ESP-IDF.md`](docs/ESP-IDF.md) | Build / flash / monitor cheatsheet |
| [`assets/README.md`](assets/README.md) | LittleFS icon pack |
| [`components/flint/board/README.md`](components/flint/board/README.md) | Board profiles |
| [`tools/README.md`](tools/README.md) | Codegen tools |

Face SDK: `components/flint/sdk/flint_face_sdk.h`

---

## Contributing

[CONTRIBUTING.md](CONTRIBUTING.md) · [SUPPORT.md](SUPPORT.md) · [SECURITY.md](SECURITY.md) · [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) · [Discussions](https://github.com/DeeTech-Labs/FlinT-OS/discussions)

---

## License

Copyright 2026 DeeTech Labs · [Apache License 2.0](LICENSE) · keep attribution in [`NOTICE`](NOTICE)

---

## Contributors

Thanks to everyone who has contributed to FlinT OS:

<p align="center">
  <a href="https://github.com/DeeTech-Labs/FlinT-OS/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=DeeTech-Labs/FlinT-OS" alt="Contributors"/>
  </a>
</p>

<p align="center">
  <sub>FlinT OS · DeeTech Labs · Apache 2.0 · ESP‑IDF</sub>
</p>
