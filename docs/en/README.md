<p align="center">
  <img src="../../.github/assets/readme-hero.en.svg" alt="FlinT OS — The firmware behind your FlinT." width="1280"/>
</p>

<p align="center">
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-PolyForm_NC-ff873e?style=flat-square&amp;labelColor=20242a" alt="PolyForm NC"/></a>
  <img src="https://img.shields.io/badge/status-alpha-c4b7aa?style=flat-square&amp;labelColor=20242a" alt="alpha"/>
  <img src="https://img.shields.io/badge/stack-ESP32--S3-c4b7aa?style=flat-square&amp;labelColor=20242a" alt="FIRMWARE / ESP32-S3"/>
</p>

<p align="center">
  <a href="#quick-start">Quick start</a> &nbsp; · &nbsp; <a href="#documentation">Documentation</a> &nbsp; · &nbsp; <a href="#license">License</a> &nbsp; · &nbsp; <a href="#contributors">Contributors</a>
</p>

<p align="center">
  <a href="../../readme">Русская версия</a> &nbsp; / &nbsp;
  <a href="https://flint.deetech.ru/">Website</a> &nbsp; / &nbsp;
  <a href="https://github.com/DeeTech-Labs">DeeTech Labs</a>
</p>

---

## What it is

**FlinT OS** is the firmware platform for **FlinT** devices (base board: **FlinT Spark**), by **DeeTech Labs**.

Core services provide hardware, time, storage, security and recovery. **Apps are Ignite packages**: install screens, watch faces, themes, languages and resources without changing firmware source.

<table>
  <tr>
    <td width="50%" valign="top">
      <h3>Built-in</h3>
      <ul>
        <li>Protected factory components · installable Ignite packages</li>
        <li>Core time service · USB/BLE PC synchronization</li>
        <li>Offline · atomic package storage and persisted consent</li>
        <li>Shared Runtime · event bus · independent recovery</li>
      </ul>
    </td>
    <td width="50%" valign="top">
      <h3>Stack</h3>
      <ul>
        <li>ESP‑IDF · LovyanGFX (slim)</li>
        <li><code>-Os</code> · no C++ exceptions / RTTI</li>
        <li>Dual application slots · protected factory volume · user storage</li>
        <li>Binary Companion over native USB and authenticated BLE</li>
      </ul>
    </td>
  </tr>
</table>

---

## Quick start

**Board:** FlinT Spark (ESP32‑S3, USB Serial/JTAG) · **ESP‑IDF** 6.0.2

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash
```

Time synchronization and package management are available through **Flint Flare**.
USB and BLE use the same binary Companion v1 contract; there is no serial-command fallback.

Optional: `idf.py -DFLINT_DEVICE=flint_spark_s3_supermini build` · board guide: [`components/flint/board/README.md`](BOARD.md)

<details>
<summary><b>Application controls</b></summary>

Use Flare's Ignite Hub to install, open and remove user packages, select clock/theme/language providers, and grant or revoke application access. Factory components cannot be removed by user packages. Recovery remains available independently of the active application.

Factory packages are provisioned on the read-only `factory` volume. User packages, settings and consent records live in writable `storage`. New applications do not require a firmware source change or reflash.

</details>

<details>
<summary><b>Hardware — FlinT Spark pinout</b></summary>

Default device revision: `flint_spark_s3_supermini`. Display: GMT028-class **ST7789** 240×280 SPI (UI landscape 280×240).

| Signal | GPIO |
|--------|:----:|
| SCK | 12 |
| MOSI (SDA) | 11 |
| CS | 10 |
| DC | 4 |
| RST | 5 |
| VCC / LED | 3.3V |
| GND | GND |

Spark uses an ESP32-S3 Super Mini. The connected development unit has verified 4 MiB Flash and 2 MiB Quad PSRAM. The Flash map provides two 1 MiB firmware slots, 192 KiB factory storage and 1728 KiB user storage; PSRAM is separate volatile memory. See [`config/README.md`](CONFIGURATION.md) for partition migration requirements. The onboard LED remains disabled until its configuration is verified.

Hardware is fixed per model/revision under `components/flint/board/models/`. Select a supported revision with `-DFLINT_DEVICE=…`.

</details>

<details>
<summary><b>Project layout</b></summary>

```
flint_os/
├── main/                 # ESP-IDF entry point
├── config/               # sdkconfig.defaults + partitions.csv
├── components/
│   ├── flint/            # hardware, links and recovery integration
│   └── LovyanGFX/        # slim hardware display component
├── platform/             # shared Core, Runtime, SDK, compiler and tests
│   ├── system/           # source factory Ignite projects
│   └── factory/          # compiled factory packages
├── tools/                # device selection and firmware tooling
└── brand/                # marketing logo
```

</details>

---

## Documentation

USB and BLE share the binary Companion contract. See the
[companion protocol](COMPANION-PROTOCOL.md) for framing, compatibility and trust.

| Doc | Contents |
|-----|----------|
| [`platform/README.md`](../../platform/readme) | Shared platform contracts, security, formats and current limits |
| [`docs/IGNITS.md`](IGNITS.md) | Binary packages and project compilation |
| [`docs/ROADMAP.md`](ROADMAP.md) | Migration milestones and hardware validation |
| [`docs/ARCHITECTURE.md`](ARCHITECTURE.md) | Core, Runtime, hardware and application boundaries |
| [`docs/ESP-IDF.md`](ESP-IDF.md) | Build and target configuration |
| [`components/flint/board/README.md`](BOARD.md) | Board profile and wiring |
| [`tools/README.md`](TOOLS.md) | Local development tools |
| [`CHANGELOG.md`](../../CHANGELOG.md) | Release history (Keep a Changelog) |

Public API, Face SDK, package format and protocol versions remain **1** during active development. Bounded Lua execution is enabled and physically validated on Spark; background services are not supported. Physical-device checks are separate from the local test suite.

---

## Contributing

[CONTRIBUTING.md](CONTRIBUTING.md) · [SUPPORT.md](SUPPORT.md) · [SECURITY.md](SECURITY.md) · [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) · [Discussions](https://github.com/DeeTech-Labs/FlinT-OS/discussions)

---

## License

Copyright 2026 DeeTech Labs · [PolyForm Noncommercial 1.0.0](../../LICENSE) · keep attribution in [`NOTICE`](../../NOTICE)

---


## Contributors

Thank you to everyone who contributes code, tests, bug reports, documentation and translations to **FlinT OS**.

<a href="https://github.com/DeeTech-Labs/FlinT-OS/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=DeeTech-Labs/FlinT-OS" alt="Contributors to FlinT OS"/>
</a>

<p><sub>FlinT OS · Part of the FlinT ecosystem · Developed by DeeTech Labs</sub></p>
