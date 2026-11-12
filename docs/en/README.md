<p align="center">
  <img src="../../resources/docs/readme-hero.en.svg" alt="FlinT OS — The firmware behind your FlinT." width="1280"/>
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
  <a href="../../README.md">Русская версия</a> &nbsp; / &nbsp;
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
        <li>ESP‑IDF · native ST7789 SPI driver</li>
        <li><code>-Os</code> · no C++ exceptions / RTTI</li>
        <li>Dual application slots · protected factory volume · user storage</li>
        <li>Binary Companion over native USB and authenticated BLE</li>
      </ul>
    </td>
  </tr>
</table>

---

## Quick start

The local `platform/` working tree contains matching touch/UI changes and build-local SDK generation; no external override is needed for a local build. These changes are uncommitted, so the previous pin alone does not reproduce this working tree. [Integration status](development/sdk-integration.md).


**Board:** FlinT Spark (ESP32‑S3, USB Serial/JTAG) · **ESP‑IDF** 6.1.0

```bash
idf.py -DSDKCONFIG=config/sdkconfig -DIDF_TARGET=esp32s3 build
idf.py -p /dev/ttyACM0 flash
```

Time synchronization and package management are available through **Flint Flare**.
USB and BLE use the same binary Companion v1 contract; there is no serial-command fallback.

Optional: `idf.py -DFLINT_DEVICE=flint_spark build` · board guide: [Spark board guide](hardware/spark-s3-supermini.md)

<details>
<summary><b>Application controls</b></summary>

Use Flare's Ignite Hub to install, open and remove user packages, select clock/theme/language providers, and grant or revoke application access. Factory components cannot be removed by user packages. Recovery remains available independently of the active application.

Factory packages are provisioned on the read-only `factory` volume. User packages, settings and consent records live in writable `storage`. New applications do not require a firmware source change or reflash.

</details>

<details>
<summary><b>Hardware — FlinT Spark pinout</b></summary>

Default model: `flint_spark`. Display: GMT028-class **ST7789** 240×280 SPI (UI landscape 280×240).

| Signal | GPIO |
|--------|:----:|
| SCK | 12 |
| MOSI (SDA) | 11 |
| CS | 10 |
| DC | 4 |
| RST | 5 |
| VCC / LED | 3.3V |
| GND | GND |

Spark uses an ESP32-S3 Super Mini. The connected development unit has verified 4 MiB Flash and 2 MiB Quad PSRAM. The Flash map provides two 1 MiB firmware slots, 192 KiB factory storage and 1728 KiB user storage; PSRAM is separate volatile memory. See [`config/README.md`](development/configuration.md) for partition migration requirements. The onboard LED remains disabled until its configuration is verified.

Hardware modules live under `src/modules/` and are selected per model in `config/devices/`. Select a supported model with `-DFLINT_DEVICE=…`.

</details>

<details>
<summary><b>Project layout</b></summary>

```
flint_os/
├── main/                 # ESP-IDF entry point
├── src/                  # hardware, input, transport, recovery, Core integration
├── config/               # defaults, board parameters, version, local sdkconfig
├── cmake/                # build logic and header templates
├── platform/             # pinned shared SDK and source factory projects
├── resources/            # branding and documentation assets
├── tools/                # developer commands and device checks
├── tests/                # host and tooling tests
├── docs/                 # topic documentation in RU/EN
└── build/                # generated headers, factory packages and images
```

</details>

---

## Documentation

USB and BLE share the binary Companion contract. See the
[companion protocol](architecture/companion-protocol.md) for framing, compatibility and trust.

| Doc | Contents |
|-----|----------|
| [`platform/README.md`](../../platform/README.md) | Shared platform contracts, security, formats and current limits |
| [Ignite packages](development/ignite-packages.md) | Binary packages and project compilation |
| [Roadmap](project/roadmap.md) | Migration milestones and hardware validation |
| [Architecture](architecture/overview.md) | Core, Runtime, hardware and application boundaries |
| [ESP-IDF build](getting-started/building.md) | Build and target configuration |
| [Spark board guide](hardware/spark-s3-supermini.md) | Board profile and wiring |
| [Tools](development/tools.md) | Local development tools |
| [`CHANGELOG.md`](../../CHANGELOG.md) | Release history (Keep a Changelog) |

Application API 1 and Companion v1 define the supported application and transport contracts. Bounded Lua execution is enabled and physically validated on Spark; background services are not supported. Physical-device checks are separate from the local test suite.

---

## Contributing

[CONTRIBUTING.md](development/contributing.md) · [SUPPORT.md](project/support.md) · [SECURITY.md](project/security.md) · [CODE_OF_CONDUCT.md](project/code-of-conduct.md) · [Discussions](https://github.com/DeeTech-Labs/FlinT-OS/discussions)

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

## Documentation index

### Getting started

- [Build and flash](getting-started/building.md).

### Architecture

- [Overview](architecture/overview.md).
- [Display / recovery](architecture/display.md).
- [BLE lifecycle](architecture/ble-lifecycle.md).
- [Input / system UI](architecture/input-and-system-ui.md).
- [Companion](architecture/companion-protocol.md).

### Hardware

- [Spark S3 Super Mini](hardware/spark-s3-supermini.md).
- [Waveshare touch preparation](hardware/waveshare-touch-preparation.md).

### Development

- [Layout and migration](development/repository-layout.md).
- [Configuration](development/configuration.md).
- [Tools](development/tools.md).
- [Ignite packages](development/ignite-packages.md).
- [Testing](development/testing.md).
- [SDK integration status](development/sdk-integration.md).

### Validation

- [Diagnostics and measurements](validation/performance.md).
- [2026-11-10 optimization report](validation/2026-11-10-optimization.md).

### Project

- [Roadmap](project/roadmap.md).
- [Changelog](../../CHANGELOG.md).
- [Contributing](development/contributing.md).
- [Security](project/security.md).
- [Support](project/support.md).
- [Code of conduct](project/code-of-conduct.md).

### Licensing

- [commercial-license](legal/commercial-license.md).
- [community-exception](legal/community-exception.md).
- [hardware-license](legal/hardware-license.md).
- [licensing](legal/licensing.md).
- [sdk-development-permission](legal/sdk-development-permission.md).
- [trademarks](legal/trademarks.md).
