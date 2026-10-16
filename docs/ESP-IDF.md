# ESP-IDF cheatsheet — FlinT OS

Target: **ESP32-C3** · ESP-IDF **v5.x / v6.x** · typical port `/dev/ttyACM0`.

## Environment

```bash
source ~/.espressif/tools/activate_idf_v6.0.2.sh   # or your install / EIM activate
idf.py --version
```

## First run / target

```bash
cd /path/to/flint_os
idf.py set-target esp32c3
idf.py reconfigure
```

Optional board profile (default **FlinT Spark** / `flint_spark`):

```bash
idf.py -DFLINT_BOARD=flint_spark build
```

## Build / size

```bash
idf.py build
idf.py size
idf.py size-components
```

Artifacts: `build/flint_os.bin`, `build/flint_os.elf`.

## Flash & monitor

```bash
idf.py -p /dev/ttyACM0 flash monitor
idf.py -p /dev/ttyACM0 app-flash monitor   # app partition only; keeps storage image
# Leave monitor: Ctrl+]
```

Full `flash` rewrites the `storage` LittleFS image. UI prefs on `cfg` are separate and survive asset reflash.

## Clean

```bash
idf.py clean
idf.py fullclean
# If third_party LovyanGFX / locks are wedged:
rm -rf build third_party/LovyanGFX
idf.py build
```

## Config & partitions

```bash
idf.py menuconfig
```

Project defaults: [`config/sdkconfig.defaults`](../config/sdkconfig.defaults)  
Partition table: [`config/partitions.csv`](../config/partitions.csv)

Local `sdkconfig` is gitignored.

## Secrets

```bash
cp components/flint/config/secrets.example.h components/flint/config/secrets.h
# edit SSID/password — never commit secrets.h
```

## Diagnostics

```bash
idf.py -p /dev/ttyACM0 monitor
idf.py -p /dev/ttyACM0 erase-flash
esptool.py --port /dev/ttyACM0 chip_id
sudo usermod -aG dialout $USER   # USB permission, then re-login
```

Core dump (partition present):

```bash
idf.py -p /dev/ttyACM0 coredump-info
```

Everyday debugging: serial monitor + `LOGI` / `LOGE`.

## Common tasks

| Task | Command |
|------|---------|
| Build | `idf.py build` |
| Flash + log | `idf.py -p /dev/ttyACM0 flash monitor` |
| App only | `idf.py -p /dev/ttyACM0 app-flash monitor` |
| Retarget | `idf.py fullclean && idf.py set-target esp32c3` |
| Flash usage | `idf.py size-components` |

## References

- [ESP-IDF Get Started (ESP32-C3)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/)
- [idf.py](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/tools/idf-py.html)
- Board profiles: `components/flint/board/README.md`
- Tools: `tools/README.md`

## Settings partition recovery

On first boot, a fully erased `cfg` partition is initialized automatically.
If an existing filesystem cannot mount, the firmware preserves its contents and
uses default settings for that boot. It does not format a damaged partition.
After backing up any data you need, explicitly erase only `cfg` to reset settings
and the saved soft clock; the next boot will initialize it again. Use the offsets
and size from the partition table of the firmware installed on the device.
