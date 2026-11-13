[English translation](../../en/development/configuration.md)

# Конфигурация прошивки

`config/sdkconfig.defaults` выбирает ESP32-S3, бинарный native USB, аутентифицированный NimBLE и LittleFS. `config/partitions.csv` рассчитан на проверенную Flash 4 МиБ: два слота приложений по 1024 КиБ, 192 КиБ защищённых заводских пакетов, 1728 КиБ пользовательского хранилища. Отдельная PSRAM 2 МиБ не меняет вместимость Flash.

При переходе со старой карты изменяются `ota_1`, `factory`, `storage`. До её записи на существующее устройство сохраните и перенесите пользовательские данные и запишите новый заводской образ. Обновления только приложения по OTA недостаточно.

Аппаратные исходники — `src/modules/st7789/`; параметры — `config/devices/flint_spark/`. Выбор устройства генерирует заголовки в `<build-dir>/generated/board/` до конфигурации компонентов. Профиля C3 нет. [Сборка](../getting-started/building.md), [плата](../hardware/spark-s3-supermini.md).

## Политика BLE

`ble_enabled` сохраняется отдельно от Companion и по умолчанию включена. После выключения доступ восстанавливается через USB. `CONFIG_FLINT_BLE_START_MEMORY_REQUIREMENT` резервирует 64 KiB internal сверх общего резерва для запуска. Жизненный цикл, worker, диагностика и ошибки описаны в [ARCHITECTURE.md](../architecture/ble-lifecycle.md).

[Размещение конфигурации и миграция](repository-layout.md). Версия: `config/version.cmake`; default ID: `config/devices.json`; генерация: `<build-dir>/generated/`.

## Конфигурация устройства

Требуются Python **3.11+** (стандартный `tomllib`) и Pillow. Единственный
редактируемый файл модели — `config/devices/<model>/device.toml`.
Начало [полного конфига Spark](../../../config/devices/flint_spark/device.toml):

```toml
name = "FlinT Spark / S3 Super Mini development"

[modules.st7789]
rotation = 3
frequency = 80000000
pins = { mosi = 11, sclk = 12, cs = 10, dc = 4, reset = 5 }
```

Неуказанный модуль отключён: его исходники и зависимости ESP-IDF не включаются
в firmware. При отсутствии display/touch остаются минимальные реализации
интерфейсов OS. Recovery без дисплея сообщает причину через logger.

В этом же файле секция `[modules.st7789.panel]` содержит размеры стекла и GRAM,
смещения, MADCTL, inversion, three-wire и полную последовательность питания/gamma.
Отдельных файлов панели и ссылок на presets нет. `rotation`, `frequency` и GPIO
задаются в `[modules.st7789]`. Сейчас реализованы только rotation 3, стекло 240×280,
GRAM 240×320 и canvas 280×240; другие геометрии отклоняются. Одинаковый размер
панелей не означает совпадение их электрических параметров и init-команд.

Для включения CST816 добавьте секцию в тот же файл. Пример синтаксиса ниже
использует условные свободные GPIO и **не является схемой подключения Waveshare**:

```toml
[modules.cst816]
pins = { sda = 1, scl = 2, reset = 6, interrupt = 7 }
raw_size = [240, 280]
swap_xy = true
# mirror_x = false
# mirror_y = false
# frequency = 400000
# address = 0x15
```

После swap размеры raw должны совпасть с canvas. Зеркалирование и реальные GPIO
нужно сверять с полученной платой. Селектор проверяет типы, неизвестные поля,
доступность GPIO в ESP32-S3, повторное назначение GPIO, размеры, ориентацию,
область GRAM и длины команд. Это не проверка схемы, питания и зарезервированных
Flash/PSRAM-пинов конкретной платы; аппаратная проверка обязательна.

Canvas берётся из поддержанной геометрии панели. Без дисплея сохраняется
viewport OS 280×240; произвольные размеры canvas пока не настраиваются.

Новая модель обнаруживается по `config/devices/<model>/device.toml` автоматически.
ID равен имени каталога: `flint_spark`. В `config/devices.json` хранится только
default ID. Подкаталог контроллера не нужен. Будущая конфигурация Spark на Waveshare
хранится рядом в `config/devices/flint_spark/device.future.toml`; селектор её игнорирует.
После получения платы нужно проверить и дополнить этот файл, затем заменить им
`device.toml`. Это будущая аппаратная база Spark, а не отдельная модель устройства.

Прежний ID `flint_spark_s3_supermini` заменён на `flint_spark`. Для существующего
CMake cache явно укажите `-DFLINT_DEVICE=flint_spark`; `select_device.py --build`
передаёт новый ID автоматически.

```sh
python3 tools/select_device.py --list
python3 tools/select_device.py --device flint_spark --build
```

`src/modules/<id>/module.json` описывает реализацию драйвера: роль, исходники и
зависимости. При добавлении платы его менять не требуется. Новый тип оборудования
требует настоящего драйвера, manifest и валидатора конфигурации в селекторе.
Нельзя включить несуществующий драйвер одной секцией TOML.

`device_config.h`, `device_display.h`, `device_touch.h` и `device.cmake`
генерируются в `<build>/generated/board/`. Модельные `device_config.h`,
`device.cmake`, `display.h` и `modules.json` больше не используются и удалены.
CMake отслеживает TOML и manifests; при переключении селектор удаляет устаревшие
заголовки отключённых устройств. Ошибочный конфиг не заменяет готовые generated
файлы. Новая конфигурация не изменяет протокол, разделы Flash или закреплённый SDK.

## Дополнительная периферия

Поддержаны `qmi8658`, `pcf85063`, `battery_adc`, `pwm_buzzer`, `pwm_backlight`,
`power_button`, а также внешние `bmp280`, `bme280`, `veml7700`. Конкретные секции,
GPIO, native API, единицы и ограничения описаны в [руководстве будущего Spark](../hardware/waveshare-touch-preparation.md)
и непосредственно в заголовках модулей. I2C-датчики используют общую SDA/SCL;
конфликты адресов запрещены. BMP280 и BME280 могут работать вместе на разных адресах.
