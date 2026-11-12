# Сервис аппаратных данных

Core единолично владеет публичным `sensors::Broker`, permissions, subscriptions и cache.
OS backend передаёт bounded desired snapshot и последний sample каждого источника (до16).
Задержавшийся Core приводит к coalescing, а не к росту памяти. Worker не исполняет Lua
и не читает Core: он только уведомляет задачу владельца о готовых данных.

Один conditional FreeRTOS worker владеет I2C, включая touch/RTC, и battery ADC. Без этих
модулей task, mailbox и cache исключены из сборки. Активный flint_spark по-прежнему
содержит только ST7789; future hardware profile не активируется. Каталог строится из
конфигурации. I2C handles и chip ID публичных sensors создаются/проверяются только по demand.

CST816 reset имеет deadlines10/200ms; IRQ будит worker, контакт читается через16ms.
Touch обслуживается между I2cDevice transactions, timeout каждой5ms. Core забирает до16
raw records с generation захвата. Overflow отменяет жест, старые generations подавляются
до отпускания. Реальная latency пока не проверена на плате.

BMP/BME: forced conversion12ms, затем sleep, minimum1000ms/default5000ms. Bosch init
выполняет одну транзакцию за worker turn. VEML7700: minimum500ms/default1000ms, gain1/8,
integration100ms, deadline210ms, shutdown после чтения. Saturation — ошибка, без autorange
и коррекции нелинейности. IMU: minimum20ms/default40ms; CTRL2/CTRL3 ODR code7 при interval
меньше36ms, code8 иначе. Точная 6DOF rate зависит от ревизии QMI8658C. Ускорение — m/s2,
диапазон +/-2g; угловая скорость — rad/s, +/-512dps. Оси device — identity sensor X/Y/Z,
без display rotation. Направление относительно корпуса/экрана и mounting calibration
аппаратно не подтверждены.

Без demand публичные sensors не опрашиваются, IMU выключается, ALS завершает текущую
конверсию и shutdown. Worker спит до IRQ/изменения snapshot/реального deadline.
При ошибке driver останавливается. Recovery увеличивает source generation и публикует
pending, инвалидируя старый good cache. До успешного sample допускаются3 попытки с
backoff1/2/4s; затем новый explicit refresh разрешает следующий цикл. Повторное чтение
не обновляет физический timestamp.

ADC имеет системный cache30s. `power` поддерживает `voltage_mv`, `age_ms`, `measured_at_ms`,
`status`; failed или voltage старше90s недоступно. RTC cache UTC экстраполируется от
монотонной метки без I2C в Core. Запись asynchronous: один pending request, token,
завершение после write/readback. STOP/неверный BCD отклоняются.

PWM остаётся Core-owned. Beep ограничен5s непрерывно, минимум100ms между командами,
паузой не короче предыдущего burst и mute (defaultfalse, хранит Core). Повторы не продлевают
burst сверх5s. PWR/BOOT: IRQ, debounce30ms, hold1000ms, focused input value1/2.
Power hold устанавливается до создания task. `deviceReleasePower` — явная операция после
quiesce storage/transports; кнопка сама питание не отключает.

Diagnostics содержат metadata: wakes/missed deadlines/max wake delay/cycle, stack watermark,
sample gaps/coalescing, counts/errors/time I2C/ADC. Это runtime counters, не измерения мощности.
Raw sensor values не выдаются в обход permissions. I2C counters охватывают I2cDevice;
raw CST816 transactions выполняет отдельный драйвер.

## Сборка и перенос

Нужны ESP-IDF6.1, ESP32-S3 и IgniteSDK ContractRevision0x20261113. SDK закреплён commit `3c8ad11` в `platform/`; configure отклоняет несовместимый SDK.
Пакеты необходимо пересобрать под несовместимую revision. Partition layout не меняется,
storage не форматируется: `/storage/packages/settings.bin` и `grants.bin` сохраняются.
Старые grants не дают новые sensor permissions автоматически: запросы проверяет Core.

## Невыполненная аппаратная проверка

Host mocks и ESP32-S3 compile/build не подтверждают физическое поведение. Проверить:
touch IRQ/latency/overflow под I2C нагрузкой; wiring/pullups/address; RTC oscillator-stop,
снятие battery, readback/reboot; ADC calibration/divider; реальные IMU ODR/оси/готовность;
disconnect/reconnect/recovery; button bounce/hold; питание по USB. Снять counters/stack,
CPU awake time и ток battery/USB для idle/no-demand, ALS, IMU20/40ms и BLE. Проверить
power loss при settings/grants/RTC writes и безопасное выключение. Эти timing, hardware
и energy проверки пока не выполнены.

Остановка PWM по deadline выполняется Core event loop: физическое превышение длительности при задержке задачи требует измерения на устройстве. Это не аппаратный таймер отсечки.

## SDK revision

SDK датчиков закреплён локальным commit `3c8ad11` в `platform/`. Обычная сборка использует этот submodule; внешний `FLINT_PLATFORM_DIR` нужен только при намеренной разработке нового SDK.
