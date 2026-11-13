# Building and flashing ESP32-S3

[Index](../README.md) · [Canonical Russian](../../ru/getting-started/building.md) · [Use/recovery](device-use.md)

Git, ESP-IDF6.1.0 (upstream tag v6.1), Python3.11+, Pillow in IDF managed Python are needed.
Install IDF using its platform installer/install script, activate export.ps1/export.sh and
check `idf.py --version`. Commands below start in OS root with matching esp32s3 toolchain.
Inspect git/submodule status first; normal SDK pin845590f requires contract0x20261114/FLM3.

```sh
git submodule update --init --recursive
python -m pip install Pillow
python tools/select_device.py --list
python tools/select_device.py --device flint_spark --build-dir build/docs-firmware --sdkconfig config/local/sdkconfig.docs --build
```

List is read-only. Build selects device.toml, generates board/SDK assets under its build
tree and runs IDF; the sdkconfig is isolated. Activated-IDF alternative:

```sh
idf.py -B build/docs-firmware -DSDKCONFIG=config/local/sdkconfig.docs -DIDF_TARGET=esp32s3 -DFLINT_DEVICE=flint_spark build size
```

Success: Project build complete, flint_os.bin/factory.bin and app slot size check. Clear an
old sibling override with explicit FLINT_PLATFORM_DIR=<OS checkout>/platform. set-target is
for intentional target changes, not every build. If the remote lacks a locally coordinated
pin, obtain the owning SDK checkout/bundle from maintainers; never substitute latest. With
provided local SDK use `git -c submodule.platform.url=<SDK path> submodule update --init platform`.
No claim is made that local commits were published.

## Flash explicitly

Check actual board revision/wiring/Flash/PSRAM and protect important existing data first.
This writes selected firmware/partition/factory, not a host test:

```sh
python tools/select_device.py --device flint_spark --build-dir build/docs-firmware --sdkconfig config/local/sdkconfig.docs --build --flash --port COM7
```

Use your actual port (/dev/ttyACM0 on Linux); do not run without your board/update plan.
Flare has no firmware updater. App-only OTA does not update factory/partitions; firmware
and factory.bin need the same SDK. Active4MiB layout has two1MiB app slots,192KiB factory,
1728KiB storage and service partitions. PSRAM is separate. Sensor/service integration changed
no partitions; intentional layout changes require separate migration/recovery, never blind erase.

Factory is read-only; nonempty corrupted storage is not autoformatted. USB carries binary
Companion, not logs: close monitors before Flare. BOOT/reset follows actual board wiring.
Active Spark has no configured touch/buttons/battery; inactive Waveshare GPIO/memory/external
sensors are unverified. Main internal stack32KiB, Lua C-call nesting40; PSRAM is not stack.
Builds do not measure heap/watermark/power/latency. Core executes UI/services, hardware worker
only hardware. [Acceptance](../hardware/sensor-service.md), [host checks](../development/testing.md).
Explicit physical smoke after creating temp/: `python tools/test_lua_device.py --port COM7
--report temp/spark-lua.json`. It changes test.lua-* packages/active application, not firmware;
it is separate from normal CTest. Other OS/IDF versions need their own validation.
