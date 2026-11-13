[English translation](../../en/development/repository-layout.md)

# Структура репозитория

```text
config/       maintained settings, board parameters, version, partition table
cmake/        build modules and generated-header templates
src/          ESP-IDF component: OS implementations and hardware integration
main/         ESP-IDF entry point
platform/     pinned IgniteSDK submodule
resources/    OS branding and documentation source assets
tools/        developer commands and hardware checks
tests/host/   OS host tests and IDF stubs
tests/tooling/ Python tooling regression tests
docs/ru/      canonical documentation by topic
docs/en/      matching English translations
build/        ignored local output, generated headers, dependency lock, factory images
```

`src/` подключается через `EXTRA_COMPONENT_DIRS`; имя компонента ESP-IDF — `src`, зависимость `main` — `REQUIRES src`. Регистрация целей остаётся в их `CMakeLists.txt`, общая логика — в `cmake/`. `idf_component.yml` и `Kconfig.projbuild` находятся у компонента. Конфигурационные файлы инструментов с фиксированным расположением остаются в корне и `.github/`.

`config/devices.json` хранит только default ID; `config/devices/<model>/device.toml` — единственный конфиг устройства. Профили обнаруживаются автоматически; параметры панели находятся в том же `device.toml`. C++-реализации дисплея и touch находятся в `src/modules/`. GPIO задаются только в конфигурации платы. Наличие заготовки в `src/` не делает её выбираемым профилем.

`config/version.cmake` — единственный источник версии. CMake создаёт `generated/config/flint_version.h`; selector создаёт `generated/board/`. Заводские пакеты SDK собираются в `factory/` внутри выбранного build directory. Общие ресурсы SDK остаются в подмодуле.

## Переход с прежней структуры

Исходники `components/flint/` перенесены в `src/`; `core/system.cpp` переименован в `core/application.cpp`. `brand/` и `.github/assets/` объединены в `resources/`. `tools/tests/` перенесены в `tests/tooling/`; Bash launcher находится в `tools/flint_tools.sh`.

Пересоздайте конфигурацию сборки после смены путей компонентов. Для сохранения прежней сборки используйте новый каталог: `idf.py -B build/layout -DSDKCONFIG=config/sdkconfig -DIDF_TARGET=esp32s3 build`. Не выполняйте `set-target` без необходимости: он сбрасывает состояние конфигурации.

По умолчанию локальные настройки хранятся в `config/sdkconfig` и переживают `idf.py fullclean`. При первом configure прежний корневой `sdkconfig` копируется туда, если нового ещё нет; оригинал сохраняется. Явный `-DSDKCONFIG=<path>` или selector `--sdkconfig <path>` выбирает отдельную конфигурацию. Новый явно выбранный файл создаётся из defaults без импорта корневого файла. `--build-dir` меняет только каталог артефактов. `set-target` намеренно сбрасывает конфигурацию и сохраняет прежний файл как `.old`; не используйте его для обычной проверки S3. Зависимости фиксируются в `<build-dir>/dependencies.lock`.

Корневые документы сохраняют полный текст. Ссылки на тематические документы и ресурсы изменились; внешние ссылки на прежние пути требуют обновления. Форматы пакетов, протокол и таблица разделов не менялись. Локальные build/IDE-файлы не удаляются автоматически.

## Генерация SDK

Реестры и factory-пакеты OS создаются общей функцией SDK `flint_generate_sdk` в `<build>/generated/sdk/include/` и `<build>/generated/sdk/factory/`. Примеры при сборке прошивки не компилируются. `factory.bin` остаётся в корне каталога сборки. Генерация и тесты не обновляют исходные `platform/include/`, `platform/factory/` и `platform/examples/`; неизменившиеся результаты не переписываются.

Штатный SDK закреплён на 56292d7 / 0x20261115. Используйте закреплённый revision из remote; не заменяйте его на latest. [Интеграция](sdk-integration.md).
