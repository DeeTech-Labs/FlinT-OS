[English translation](../../en/development/testing.md)

# Проверка изменений

Рабочее дерево `platform/` содержит согласованные touch/UI-изменения SDK и генерацию в каталог сборки; внешний override для локальной сборки не нужен. Эти правки пока не закоммичены, поэтому один прежний pin не воспроизводит рабочее дерево. [Состояние интеграции](sdk-integration.md).


Команды выполняются из корня в подходящем окружении C++/Python. Используйте отдельные каталоги сборки для OS host tests и закреплённого SDK.

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

Прошивка: [сборка](../getting-started/building.md). Host-тесты не проверяют USB/BLE, display, sleep, wakeup и Flash на физическом устройстве. Скрипты `tools/test_lua_device.py` и `tools/check_install_responsiveness.py` выполняются отдельно и изменяют тестовые пакеты на подключённой плате.
