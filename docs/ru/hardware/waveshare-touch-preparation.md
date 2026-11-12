[English translation](../../en/hardware/waveshare-touch-preparation.md)

# Подготовка Waveshare ESP32-S3-Touch-LCD-1.69

Это неактивная заготовка, не поддерживаемый профиль для прошивки. Устройство пока
не получено; электрическая работа, ориентация touch и энергопотребление не проверены.
Выбранный `flint_spark_s3_supermini` и его файлы конфигурации не меняются.

По [документации Waveshare](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69)
touch-контроллер — CST816T, адрес I2C `0x15`, SDA GPIO11, SCL GPIO10,
reset GPIO13, interrupt GPIO14; дисплей имеет разрешение 240×280.
Не следует путать эту плату с отдельным модулем 1.69inch Touch LCD на CST816S.
В документации встречаются разные ревизии платы: перед подключением нужно
сверить маркировку и схему полученного экземпляра.

`src/board/models/waveshare/esp32_s3_touch_lcd_169/touch.h` содержит
эти параметры и **raw portrait identity transform**. Он не подключён активной
платой и не задаёт проверенную ориентацию для landscape canvas OS.
Полный профиль требует настройки ST7789V2, Flash/PSRAM, питания, GPIO,
полярности подсветки и согласования геометрии canvas; эти настройки не добавлены.

Новый `input/cst816.cpp` независимо реализует чтение пяти байт с регистра `0x02`
(число контактов и 12-bit X/Y), проверяет event bits и принимает single-contact
данные. Регистр `0xFA=0x60` включает periodic contact и state-change interrupts.
Основание для регистров — [пример производителя](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.69/tree/main/examples/arduino/libraries/Arduino_DriveBus/src/touch_chip)
и [включённый производителем SensorLib](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.69/blob/main/examples/arduino/libraries/SensorLib/src/touch/TouchDrvCST816.cpp).
Исходники библиотек не включаются в firmware.

Драйвер использует новый I2C master API ESP-IDF, timeout передачи 5 мс,
не создаёт задачу и не отключает auto sleep контроллера. IRQ только будит владельца;
чтение и обработка выполняются задачей Core. Общий I2C bus можно передать в `begin`:
драйвер удаляет только свой device, не переданный bus. При собственном bus
нужны внешние pull-up резисторы. GPIO ISR service общая для системы и не
удаляется при `stop`; собственный pin handler удаляется. Если существующая
ISR service использует IRAM, callback и его данные также должны быть доступны
при отключённом flash cache.

После получения платы нужны проверки касаний четырёх углов, удержания, движения,
отпускания, отмены при смене приложения, восстановления после I2C error и влияния
на idle power. До этих проверок заготовку нельзя считать hardware-validated.
