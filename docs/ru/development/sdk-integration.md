[English translation](../../en/development/sdk-integration.md)

# Интеграция SDK и touch/UI

На 12 ноября 2026 года OS закрепляет опубликованный SDK `9d768efc36df3c974c57c1d02211af1d1875bea7` (`ContractRevision=0x20261112`, [SDK PR #6](https://github.com/DeeTech-Labs/Flint-IgniteSDK/pull/6)). OS собирается с `platform/` без внешнего override и незакоммиченных копий SDK.

```sh
idf.py -B build/sdk-integration -DSDKCONFIG=config/sdkconfig -DIDF_TARGET=esp32s3 build
```

Генерация принадлежит SDK: `cmake/FlintGenerate.cmake` создаёт реестры и пакеты в `<build>/generated/sdk/`. OS использует сгенерированные заголовки перед исходными и формирует `factory.bin` из этих же системных пакетов. Примеры при сборке прошивки пропускаются; исходники SDK не перезаписываются. Подробности: [руководство SDK](../../../platform/docs/ru/SDK.md#генерация-без-изменения-исходников).

Для воспроизведения на другом ПК инициализируйте закреплённый SDK: `git submodule update --init --recursive`. Согласованный Flare использует ту же ревизию. Подробности проверки платы: [отчёт 2026-11-12](../validation/2026-11-12-icon-ui-resources.md).

Контракт использует FLM2 и FLIM v2; прежние пакеты нужно пересобрать. Бинарный framing протокола и таблица разделов сохранены. Application-only OTA не обновляет заводские пакеты: прошивка и `factory.bin` должны происходить из одного дерева SDK.
