[Русский оригинал](../../ru/development/testing.md)

# Change validation

Normal builds use pinned platform/8d53e1b with contract0x20261115/FLM3; sibling overrides are explicit. [Integration status](sdk-integration.md).


Run from the repository root in a configured C++/Python environment. OS host tests and the pinned SDK use separate build directories.

```sh
python3 -m unittest discover -s tests/tooling -v
python3 tools/check_docs.py
cmake -S tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host --parallel 2 --config Debug
ctest --test-dir build/host --output-on-failure -C Debug
cmake -S platform -B platform/build/docs-sdk -DCMAKE_BUILD_TYPE=Debug
cmake --build platform/build/docs-sdk --parallel 2 --config Debug
ctest --test-dir platform/build/docs-sdk --output-on-failure -C Debug
```

Firmware: [build](../getting-started/building.md). Host tests do not validate physical USB/BLE, display, sleep, wakeup or Flash. Run `tools/test_lua_device.py` and `tools/check_install_responsiveness.py` separately; they modify test packages on the connected board.
