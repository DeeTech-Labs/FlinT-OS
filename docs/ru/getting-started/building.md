# Сборка и прошивка ESP32-S3

[Индекс](../README.md) · [English](../../en/getting-started/building.md) · [Использование/recovery](device-use.md)

## Условия

Нужны Git, ESP-IDF 6.1.0 (upstream release tag `v6.1`), Python 3.11+ и Pillow в
управляемом Python окружении IDF. Установите ESP-IDF его штатным installer/install
script для своей ОС, затем активируйте `export.ps1` (PowerShell) или `export.sh`
(Linux/macOS). `idf.py --version` должен показать ESP-IDF v6.1, target — esp32s3.
Это инструкция для текущего исходного дерева; сборка других IDF/ОС требует проверки.

Команды выполняются из корня OS. Сначала проверьте `git status` и `git submodule status`.
Штатный SDK pin — `56292d7`, contract `0x20261115`/FLM3. Инициализация:

```sh
git submodule update --init --recursive
python -m pip install Pillow
python tools/select_device.py --list
python tools/select_device.py --device flint_spark --build-dir build/docs-firmware --sdkconfig config/local/sdkconfig.docs --build
```

`--list` ничего не пишет. Build выбирает единственный `device.toml`, генерирует board/SDK
assets в build tree и вызывает IDF. Локальный sdkconfig отделён от других сборок.
Альтернатива в активированном IDF:

```sh
idf.py -B build/docs-firmware -DSDKCONFIG=config/local/sdkconfig.docs -DIDF_TARGET=esp32s3 -DFLINT_DEVICE=flint_spark build size
```

Успех — `Project build complete`, `build/docs-firmware/flint_os.bin`, `factory.bin` и
проверка размера app slot. CMake не должен использовать случайный sibling SDK: при старом
cache сбросьте override явно `-DFLINT_PLATFORM_DIR=<OS checkout>/platform`. Не применяйте
`set-target` к рабочему build без необходимости: это смена конфигурации, не шаг каждой сборки.

Если remote ещё не содержит локально согласованный SDK SHA, `submodule update` не сможет
его скачать. Получите owning SDK checkout/bundle той же ревизии от сопровождающего;
не заменяйте pin на latest ради configure. Для предоставленного локального checkout:
`git -c submodule.platform.url=<путь-к-SDK> submodule update --init platform`.
Это не утверждение, что локальные коммиты опубликованы в origin.

## Прошивка — отдельное действие

До подключения проверьте электрическую ревизию, Flash/PSRAM и wiring активного конфига.
Для уже работающего устройства сначала подготовьте сохранение важных данных. Следующая
команда **пишет firmware/таблицу/factory по выбранному build**, это не host test:

```sh
python tools/select_device.py --device flint_spark --build-dir build/docs-firmware --sdkconfig config/local/sdkconfig.docs --build --flash --port COM7
```

Замените COM7 своим портом (`/dev/ttyACM0` на Linux). Не выполняйте команду без своей платы
и согласованного плана обновления. Flare не имеет firmware updater. Application-only OTA
не обновляет factory/partitions; firmware и factory.bin должны использовать один SDK contract.
Активная карта Flash 4 MiB: два app slot по 1 MiB, factory 192 KiB, storage 1728 KiB,
служебные разделы. PSRAM отдельна от Flash. Интеграция датчиков/служб не меняла partitions;
при намеренной смене layout требуется отдельная миграция/recovery, а не erase_flash.

## Эксплуатационные границы

Factory read-only, storage содержит пакеты/настройки/grants; повреждённый непустой том
не форматируется автоматически. Native USB занят бинарным Companion, не serial logging;
закройте monitor перед Flare. Recovery/BOOT/reset следуют проверенной электрической схеме.
Текущий Spark display-only: touch/buttons/battery не сконфигурированы. Future Waveshare
неактивен, включая отличающиеся ревизии GPIO/Flash/PSRAM и необязательные внешние датчики.

Основной stack 32 KiB internal, Lua C-call nesting limit40; PSRAM не заменяет task stack.
Проверка сборки не измеряет heap, stack watermark, ток или touch/USB/BLE latency.
SDK Core исполняет UI/service VM, OS worker только аппаратные операции; исходники и
лимиты общей Lua берутся из pin. [Аппаратная приёмка](../hardware/sensor-service.md).

Host проверки — [testing](../development/testing.md). Явный hardware smoke, если плата
уже подготовлена: `python tools/test_lua_device.py --port COM7 --report temp/spark-lua.json`.
Заранее создайте temp/. Он изменяет test.lua-* пакеты/активное приложение, не прошивает и
не стирает Flash; это всё равно отдельный hardware run, не часть обычного CTest.
