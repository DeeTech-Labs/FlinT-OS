[English translation](../en/TOOLS.md)

# Локальные инструменты

Все команды выполняются из корня OS.

- `tools/select_device.py` выбирает сопровождаемый S3-профиль; сборка, прошивка и монитор запускаются по соответствующим явным параметрам.
- `platform/tools/compile_ignite.py` компилирует JSON-проект и ресурсы в бинарный Ignite.
- `platform/tools/build_system.py` собирает заводские Ignite.
- `platform/tools/generate_registry.py` создаёт реестры API и разрешений.

Старые текстовый клиент синхронизации времени, генератор наборов ресурсов и экспортёр рендерера удалены вместе с прежними контрактами. `platform/` — закреплённый подмодуль SDK; инициализация: `git submodule update --init --recursive`. Экспорта исходников нет. [Сборка](ESP-IDF.md), [пакеты](IGNITS.md).
