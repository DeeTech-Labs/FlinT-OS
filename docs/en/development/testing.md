[Русский оригинал](../../ru/development/testing.md)

# Change validation

The local `platform/` working tree contains matching touch/UI changes and build-local SDK generation; no external override is needed for a local build. These changes are uncommitted, so the previous pin alone does not reproduce this working tree. [Integration status](sdk-integration.md).


Run from the repository root in a configured C++/Python environment. OS host tests and the pinned SDK use separate build directories.

```sh
python3 -m unittest discover -s tests/tooling -v
python3 tools/check_docs.py
cmake -S tests/host -B build-host -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host --parallel 2
ctest --test-dir build-host --output-on-failure
cmake -S platform -B build-platform -DCMAKE_BUILD_TYPE=Debug
cmake --build build-platform --parallel 2
ctest --test-dir build-platform --output-on-failure
```

Firmware: [build](../getting-started/building.md). Host tests do not validate physical USB/BLE, display, sleep, wakeup or Flash. Run `tools/test_lua_device.py` and `tools/check_install_responsiveness.py` separately; they modify test packages on the connected board.
