[English translation](../../en/development/tools.md)

# Локальные инструменты

Все команды выполняются из корня OS.

- `tools/select_device.py` выбирает сопровождаемый S3-профиль; сборка, прошивка и монитор запускаются по соответствующим явным параметрам.
- `platform/tools/compile_ignite.py` компилирует JSON-проект и ресурсы в бинарный Ignite.
- `platform/tools/build_system.py` собирает заводские Ignite.
- `platform/tools/generate_registry.py` создаёт реестры API и разрешений.

Старые текстовый клиент синхронизации времени, генератор наборов ресурсов и экспортёр рендерера удалены вместе с прежними контрактами. `platform/` — закреплённый подмодуль SDK; инициализация: `git submodule update --init --recursive`. Экспорта исходников нет. [Сборка](../getting-started/building.md), [пакеты](ignite-packages.md).

[Размещение конфигурации и миграция](repository-layout.md). Версия: `config/version.cmake`; default ID: `config/devices.json`; генерация: `<build-dir>/generated/`.

Bash launcher: `bash tools/flint_tools.sh`. Проверка ссылок: `python3 tools/check_docs.py`. Selector: `--build-dir <path>` выбирает каталог артефактов, `--sdkconfig <path>` — постоянную локальную конфигурацию (по умолчанию `config/sdkconfig`). Без action selector только генерирует заголовки; при build их создаёт CMake; monitor не генерирует файлы.
