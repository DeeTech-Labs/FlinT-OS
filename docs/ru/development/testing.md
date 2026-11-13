[English translation](../../en/development/testing.md)

# Проверка изменений

Штатный SDK закреплён на `56292d7` / `0x20261115`; он уже включает службы и датчики. [Согласование SDK](sdk-integration.md).


Команды выполняются из корня в подходящем окружении C++/Python. Используйте отдельные каталоги сборки для OS host tests и закреплённого SDK.

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

Прошивка: [сборка](../getting-started/building.md). Host-тесты не проверяют USB/BLE, display, sleep, wakeup и Flash на физическом устройстве. Скрипты `tools/test_lua_device.py` и `tools/check_install_responsiveness.py` выполняются отдельно и изменяют тестовые пакеты на подключённой плате.
