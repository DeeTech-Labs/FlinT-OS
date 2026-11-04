[English translation](../en/ARCHITECTURE.md)

# Архитектура FlinT OS

Сопровождаемая общая спецификация находится в [Flint IgniteSDK](../../platform/README.md). `platform/` — подмодуль SDK на закреплённом коммите, общий с Flare. Инициализация: `git submodule update --init --recursive`. Общие изменения вносятся в репозиторий SDK.

Аппаратный слой: `components/flint/board/`, `display/`, `ble/`, `core/system.cpp`. Переносимые службы: `platform/src/` и `platform/include/flint/`. Заводские приложения: `platform/system/`, результат компиляции — `platform/factory/`. Контракты разработки: `platform/spec/` и сгенерированные реестры.
