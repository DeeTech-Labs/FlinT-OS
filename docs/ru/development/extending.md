# Как расширять прошивку и аппаратные модули

[Оглавление](../README.md) · [English](../../en/development/extending.md) · [Конфигурация](configuration.md)

## Подготовка

Сначала воспроизведите [build](../getting-started/building.md) и [host/tooling tests](testing.md).
Проверьте активный `device.toml`, datasheet и электрическую ревизию платы. Future-файл
не является активной схемой. Не меняйте Flash layout/PSRAM/GPIO только ради успешного build.
Shared API/permissions/Runtime меняются в owning SDK, не в `platform/` как скрытая локальная правка.

## Добавить модуль

1. Найдите аналог в `src/modules/` (BME280/BMP280/VEML7700 для I2C one-shot).
   Драйвер — `flint::modules`, общие bus/PWM/compensation — `flint::hardware`.
   Добавьте модульные source/header и `module.json`: source paths и IDF dependencies.
2. В `tools/select_device.py` расширьте `load_device`/`generated_headers`: строгие allowed/
   required fields, типы, диапазоны, GPIO conflicts, адреса, shared bus constraints и generated
   macros. Само наличие каталога не делает произвольный TOML поддержанным.
3. Добавьте тесты в `tests/tooling/test_device.py`: выбранный модуль присутствует, невыбранный
   отсутствует вместе с IDF requires; invalid keys/pins/configs отклоняются. Смотрите
   `src/CMakeLists.txt`, который использует generated FLINT_MODULE_SOURCES/IDF_REQUIRES.
4. Единственная редактируемая конфигурация модели — `config/devices/<id>/device.toml`.
   Default model хранится в `config/devices.json`. Не создавайте отдельные panel headers/
   controller presets. Не активируйте отсутствующее оборудование для удовлетворения запроса API.
5. Подключите драйвер в `src/hardware/peripherals.cpp` под generated feature guards.
   Общая I2C-шина создаётся только при выбранных клиентах. Все её операции, включая touch/RTC,
   выполняет один hardware worker. Core не должен сохранять старый параллельный driver object.

Без выбранного модуля не должно быть driver/dependency, probing, bus resource, recovery или
периодического wakeup. Возвращайте `Error::not_supported`. Выбранный, но неисправный источник —
`source_unavailable` со state/reason; conversion/init — pending/not_ready. Не подставляйте нули.

## Подключить показания к Sensor Broker

Используйте `sensors::Backend` и bounded mailbox `src/hardware/sensor_mailbox.h`:
Core передаёт desired demand, worker — coherent snapshots. Transport buffers не второй
публичный cache. Source ID/priority, channels, SI units, ranges, axis transform и temperature
kind задаются backend. Source0 в app выбирает источник по общему SDK contract; не связывайте
Lua API с chip name/GPIO/I2C register. Связанные BME readings сохраняют sample identity.

Worker объединяет OS demand и всех разрешённых instances. Он ждёт IRQ/deadline, не делает
task на каждый датчик и не вызывает Lua. Touch приоритетен; init/recovery разбиваются на
состояния/deadlines, иначе один failed chip блокирует bus. После последнего consumer —
sleep/shutdown; reset меняет source generation, timestamps не обновляются чтением cache.
Flash может задержать даже worker: фиксируйте gaps/jitter/missed deadlines, не обещайте RT.
[Policy, RTC/brightness/battery/buttons/beep](../hardware/sensor-service.md),
[канонический Broker contract](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/ru/SENSORS.md).

## Добавить системного потребителя или policy

`src/core/application.cpp` связывает Platform callbacks с OS. Системный demand проходит
через Broker, не отдельный polling. Background admission использует current internal heap/
largest block и reserve, model `[background]`; не подменяйте internal/DMA memory PSRAM.
Permission checks остаются в SDK Core; hardware worker не выдаёт grants.
Low-voltage policy использует fresh cached voltage, не SOC/charging. Background callbacks
не должны рендерить/включать дисплей. [SDK scheduler](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/ru/PLATFORM-GUIDE.md).

## Проверить результат

Запустите tooling, соответствующие tests/host (timeouts, calibration, saturation/recovery),
SDK broker/sensor_core/background_core, затем ESP32-S3 build с выбранными и невыбранными
модулями. Проверьте generated/board/device.cmake: исходники/dependencies действительно исключены.
Не используйте future TOML как доказательство физической работоспособности.

Аппаратный отчёт: маркировка/revision, питание/pull-ups, I2C/ADC counters/durations, touch
под sampling/Flash, heap/PSRAM/stack watermark, sample age/gaps/coalescing, BLE/USB/install,
sleep/shutdown/recovery и ток idle/display/sampling. Power-cut tests выполняются отдельно
с защищёнными данными. Для service count2 нужен physical admission/latency/power report;
host pool test не выбирает окончательное число служб для модели.
