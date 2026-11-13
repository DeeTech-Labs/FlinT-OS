[English translation](../../en/development/sdk-integration.md)

# Интеграция SDK и touch/UI

OS использует SDK `ContractRevision=0x20261115`, закреплённый commit `56292d7` в `platform/`. Обычная сборка использует submodule. Для намеренной разработки SDK допускается явный `FLINT_PLATFORM_DIR`. См. [сервис датчиков](../hardware/sensor-service.md).

```sh
idf.py -B build/sdk-integration -DSDKCONFIG=config/sdkconfig -DIDF_TARGET=esp32s3 build
```

Генерация принадлежит SDK: `cmake/FlintGenerate.cmake` создаёт реестры и пакеты в `<build>/generated/sdk/`. OS использует сгенерированные заголовки перед исходными и формирует `factory.bin` из этих же системных пакетов. Примеры при сборке прошивки пропускаются; исходники SDK не перезаписываются. Подробности: [руководство SDK](../../../platform/docs/ru/SDK.md#генерация-без-изменения-исходников).

Для воспроизведения на другом ПК инициализируйте закреплённый SDK: `git submodule update --init --recursive`. Flare закрепляет тот же SDK commit. Порядок проверок: [тестирование](testing.md); локальные результаты сохраняйте в `temp/` и отчёте задачи.

Контракт использует FLM3 и FLIM v2; прежние пакеты нужно пересобрать. Бинарный framing протокола и таблица разделов сохранены. Application-only OTA не обновляет заводские пакеты: прошивка и `factory.bin` должны происходить из одного дерева SDK.
