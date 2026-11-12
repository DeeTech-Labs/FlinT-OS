[English translation](../../en/development/configuration.md)

# Конфигурация прошивки

`config/sdkconfig.defaults` выбирает ESP32-S3, бинарный native USB, аутентифицированный NimBLE и LittleFS. `config/partitions.csv` рассчитан на проверенную Flash 4 МиБ: два слота приложений по 1024 КиБ, 192 КиБ защищённых заводских пакетов, 1728 КиБ пользовательского хранилища. Отдельная PSRAM 2 МиБ не меняет вместимость Flash.

Новая карта сдвигает `ota_1`, `factory`, `storage`. До её записи на существующее устройство сохраните и перенесите пользовательские данные и запишите новый заводской образ. Обновления только приложения по OTA недостаточно.

Аппаратные исходники — `src/board/models/flint_spark/s3_supermini/`; параметры — `config/devices/flint_spark/s3_supermini/`. Выбор устройства генерирует заголовки в `<build-dir>/generated/board/` до конфигурации компонентов. Профиля C3 нет. [Сборка](../getting-started/building.md), [плата](../hardware/spark-s3-supermini.md).

## Политика BLE

`ble_enabled` сохраняется отдельно от Companion и по умолчанию включена. После выключения доступ восстанавливается через USB. `CONFIG_FLINT_BLE_START_MEMORY_REQUIREMENT` резервирует 64 KiB internal сверх общего резерва для запуска. Жизненный цикл, worker, диагностика и ошибки описаны в [ARCHITECTURE.md](../architecture/ble-lifecycle.md).

[Размещение конфигурации и миграция](repository-layout.md). Версия: `config/version.cmake`; реестр плат: `config/devices.json`; генерация: `<build-dir>/generated/`.
