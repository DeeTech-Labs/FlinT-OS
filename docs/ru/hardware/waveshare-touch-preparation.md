[English translation](../../en/hardware/waveshare-touch-preparation.md)

# Будущий FlinT Spark на Waveshare ESP32-S3-Touch-LCD-1.69

Полная заготовка аппаратного состава находится в
[device.future.toml](../../../config/devices/flint_spark/device.future.toml).
Селектор её игнорирует; активный Spark не изменён. Плата ещё не получена:
компиляция и host-тесты не подтверждают электрическую работу и энергопотребление.

Состав и основная распиновка взяты из [руководства Waveshare](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69)
и [официальных примеров](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.69).

| Узел | Реализация OS | Подключение в будущем конфиге |
|---|---|---|
| ST7789V2, 240×280 | `flint::modules::St7789` | MOSI7, SCLK6, CS5, DC4, RST8 |
| CST816T | `flint::modules::Cst816` | I2C 0x15, RST13, INT14 |
| QMI8658C | `flint::modules::Qmi8658` | I2C 0x6b, INT38 |
| PCF85063A | `flint::modules::Pcf85063` | I2C 0x51, INT39 |
| Измерение аккумулятора | `flint::modules::BatteryAdc` | ADC1/GPIO1, делитель 200k/100k, ×3 |
| Подсветка | `flint::modules::PwmBacklight` | GPIO15, LEDC timer1/channel1 |
| Зуммер | `flint::modules::PwmBuzzer` | GPIO42, LEDC timer0/channel0 |
| Питание и кнопки | `flint::modules::PowerButton` | SYS_EN41, PWR40, BOOT0 |

Touch/IMU/RTC разделяют SDA11/SCL10 и один bus ESP-IDF. Каждый драйвер владеет
только своим I2C device. Общий bus живёт до перезапуска firmware. Внешние
pull-ups обязательны; на схеме платы они предусмотрены. Нельзя назначать эту
же пару GPIO другим функциям или два устройства на один I2C-адрес.

## Ревизии и автономные цепи

[Схема V2.1](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.69/ESP32-S3-Touch-LCD-1.69_V2.1.pdf)
отличается от основной таблицы руководства: buzzer=39, RTC_INT=41, SYS_EN=42.
В Arduino-примере также есть старая комбинация PWR36/SYS_EN35/buzzer33.
Перед активацией обязательно сверить маркировку и схему полученного экземпляра.
Будущий конфиг явно использует основную таблицу руководства, а не автоопределение ревизии.

ETA6098 заряжает аккумулятор автономно, ME6217 формирует 3.3V; программного
I2C-интерфейса управления ими в схеме нет. Поэтому выдуманного драйвера зарядника,
процента заряда и charging status нет. RST идёт на CHIP_PU, а не на GPIO.
Flash 16 MB и octal PSRAM 8 MB требуют отдельной проверки sdkconfig; TOML не меняет
существующую карту разделов 4 MB и настройки Quad PSRAM активного Spark.

## Использование модулей

Каждый заголовок `src/modules/<id>/<id>.h` содержит назначение, единицы, порядок
вызовов, ограничения и пример. Пространство имён: `flint::modules`; общие I2C/PWM
и Bosch-компенсация — `flint::hardware`. Новый module включается только секцией
в одном device.toml. Исходники и IDF dependencies невыбранных драйверов не собираются.

`hardware/peripherals.h` связывает выбранные драйверы с задачей Core:

- `deviceHardwareBegin()` удерживает питание, инициализирует периферию и оставляет
  IMU/зуммер/подсветку выключенными. Дисплей включает подсветку после заполнения GRAM.
- `deviceMotionEnable/Read` даёт raw ускорение/угловую скорость с фиксированными
  диапазонами; нет фонового опроса, fusion, автоматического WoM или обработчика IMU INT.
- `deviceRtcRead/Set` читает/записывает UTC; oscillator-stop и неверный BCD отклоняются.
  Неудачная запись оставляет STOP, чтобы частично записанное время не считалось валидным.
  RTC не синхронизируется автоматически с Core epoch; alarm/wakeup не реализованы.
- `Platform::power` поддерживает key `voltage_mv` через существующий permission boundary.
  Измерение — калиброванный ADC, без подстановки значений при ошибках.
- `deviceTone(Hz, durationMs)` ограничен 5 секундами; Core учитывает deadline остановки.
  Таймеры LEDC при выключенном выходе приостановлены. `deviceBacklight(0..100)` задаёт яркость.
- `deviceReleasePower()` разрешено вызывать из Core только после завершения записей
  Flash и остановки транспорта. Кнопки пока читаются как raw diagnostics, без UI/debounce.

Новых Lua API и обхода разрешений нет: IMU/RTC/звук доступны native-интеграции,
готовность и измеренные данные — существующей диагностике. INT RTC/IMU в конфиге
резервируются; автоматический wake по ним не реализован.

## Внешние BMP280, BME280 и VEML7700

На самой Waveshare этих датчиков нет. В конце будущего TOML есть закомментированные
секции; включать их следует только после подключения и проверки питания/pull-ups.
BMP280/BME280 используют 0x76/0x77, VEML7700 — 0x10. При совместном подключении
BMP280 и BME280 им нужны разные адреса.

Общие калибровка и компенсация BMP280/BME280 реализованы по
[даташиту Bosch BMP280](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf)
и [даташиту BME280](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf).
`deviceEnvironmentStart(EnvironmentSensor::bmp280/bme280)` запускает одну forced
конверсию x1; Core завершает её по deadline через 12 мс. `deviceEnvironmentRead`
возвращает сохранённый результат: °C, Pa и %RH только у BME280. Нет busy-wait,
постоянного normal mode, высоты и sea-level correction.

`deviceLightStart/Read` работают аналогично. По [application note Vishay](https://cdn.sparkfun.com/assets/8/7/4/4/1/VEML7700_Application_Note.pdf)
используются gain 1/8 и integration 100 ms; чтение запланировано через 210 мс с
запасом на первый sample. ALS выключается после чтения, даже при ошибке I2C.
Результат — линейные lux, 0.4608 lx/count. Autorange и коррекция нелинейности
при сильном освещении пока не реализованы; saturation возвращает ошибку.
Диагностика показывает последний успешный результат; периодический сбор не запускается.

## Проверка на полученной плате

Нужны проверка ревизии GPIO, Flash/PSRAM, цветов/ориентации дисплея и четырёх углов
touch, точности RTC и батарейного напряжения, знаков осей IMU, PWM, кнопок и отключения
питания. Отдельно измерить idle power и поведение при отказах I2C/ADC.
