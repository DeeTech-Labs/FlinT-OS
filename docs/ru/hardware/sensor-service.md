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

Нужны ESP-IDF6.1, ESP32-S3 и IgniteSDK ContractRevision0x20261114. SDK закреплён commit `18d22f4` в `platform/`; configure отклоняет несовместимый SDK.
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

SDK датчиков закреплён локальным commit `18d22f4` в `platform/`. Обычная сборка использует этот submodule; внешний `FLINT_PLATFORM_DIR` нужен только при намеренной разработке нового SDK.


## Фоновые service VM: модельная политика

`[background]` — отдельная от hardware policy секция device.toml. Для Spark установлены
предварительные пределы: max_services=2, общий declared memory budget131072B,
foreground reserve49152B. Это целевой предел допуска, не гарантия одновременной работы
двух VM на физической плате. Отсутствующая секция или max_services=0 отключает API;
Core не создаёт service slots. Отдельных FreeRTOS tasks на сервисы нет: Core исполняет
VM кооперативно, с общим source cache/cooldown Sensor Broker и InstanceKey владельца.

Окно100ms:16384 instructions/64 native calls, quantum4096/16, wall budget4000us,
callback2000us/native call1000us. Минимумы: timer100ms, sensor100ms, durable state flush60000ms.
Генератор отклоняет неизвестные поля, неверные типы/диапазоны и budgets, где quantum
превышает окно или callback/native wall exceeds enclosing budget. Реальные wall counters
используют esp_timer, а instruction count не подменяет wall-clock limit.

Admission читает текущий internal heap и largest contiguous block, вычитает
CONFIG_FLINT_INTERNAL_MEMORY_RESERVE, foreground reserve и дополнительный32768B резерв
во время BLE startup. Existing VM allocations уже отражены в free heap; отдельный
общий policy budget ограничивает declared memory всех services. PSRAM не используется
как обещание дополнительной internal service memory. Отказ admission сохраняет возможность
foreground запуска; успешный запуск двух VM необходимо проверить под BLE/UI/Flash нагрузкой.

При выбранном battery ADC low-energy policy использует только свежий cached voltage_mv,
порог3400mV (переопределяется background.low_voltage_mv). Это консервативный порог напряжения,
не процент SOC и не оценка оставшегося времени. Failed/stale/unknown voltage не трактуется
как low-energy. Без battery module callback отсутствует. Pause/resume/reason принадлежат Core.

Factory app Services показывает actual service rows (по3 на странице), state/reason,
Start/Stop, Pause/Resume, Auto и Revoke. Контролы зависят от состояния и execute grant;
действия выполняются на input release. Смена выбранной identity/action между press/release
отменяет команду. На устройстве можно остановить service и отозвать execute grant без Flare.
Управление требует проверенного установленного factory package и system.diagnostics;
обычный UI или service VM не получает эти права из package ID. Компонент требует
CapBackground, поэтому не появляется на неподдерживаемых моделях.

Локальные проверки config bounds и sandbox UI с mock service rows не заменяют проверку
живых VM на ESP32-S3. Измерить2VM admission, peak/fragmentation heap, Core wakeups,
wall/CPU under BLE/UI load и ток при running/paused/no-services. Проверить энергопорог
с реальной battery calibration и power-cycle/autostart/grant revocation. Эти аппаратные
измерения пока не выполнены; значение max_services=2 остаётся предварительным.


## Статический размер проверенной сборки

13.11.2026 выполнена сборка активного flint_spark (только ST7789, background policy2)
с ESP-IDF6.1.0, ESP32-S3 и закреплённым IgniteSDK 18d22f4 / ContractRevision0x20261114 через
штатный submodule platform. Factory image включает Services. Результат `idf.py build size`:

| Показатель | Байты |
| --- | ---: |
| flint_os.bin с padding | 803760 |
| image sections total из linker map | 803650 |
| Flash Code | 523884 |
| Flash Data | 171636 |
| Статический DIRAM | 105142 |
| Остаток app partition1MiB | 244816 |

Статический DIRAM остался105142B относительно проверенной сборки до background integration:
.text72811B, .data18899B, .bss13432B. Это результат linker map/esp_idf_size, **не измерение
runtime heap** и не доказательство, что две service VM помещаются в RAM. BackgroundState,
Broker внутри Core, Lua VM, queues и dynamic caches выделяются при исполнении; для них
необходимы реальные free/largest-block/peak measurements под нагрузкой. Значения относятся
к указанной сборке, не являются обещанием для другого toolchain/config/SDK revision.
Логи локальной проверки: temp/background-idf-final.log и temp/background-idf-size.json.
