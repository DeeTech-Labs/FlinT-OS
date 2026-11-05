[English translation](../en/BOARD.md)

# FlinT Spark S3 Super Mini

Поддерживается только профиль `flint_spark_s3_supermini`, цель `esp32s3`. Профиль C3 удалён; выбор его ID завершается ошибкой.

## Подключение дисплея

ST7789 240×280, альбомный UI 280×240: SCLK — GPIO12, MOSI/SDA — GPIO11, CS — GPIO10, DC — GPIO4, RST — GPIO5. MISO и программное управление подсветкой не используются. Индикатор состояния отключён: конфигурация встроенного LED не проверена. Native USB использует GPIO19/20, не назначенные дисплею.

Дисплей не использует GPIO0/3/45/46 (strapping), GPIO19/20 (USB) и выводы памяти. Подключение требует визуальной проверки на устройстве.

## Проверенная плата

4 ноября 2026 года определена ESP32-S3 v0.2 со встроенной XMC Flash 4 МиБ и AP_3v3 PSRAM 2 МиБ. Настройки используют карту Flash 4 МиБ, Quad PSRAM 40 МГц и тест памяти при загрузке. Другие платы с тем же торговым названием могут отличаться.

```sh
python3 tools/select_device.py --list
python3 tools/select_device.py --device flint_spark_s3_supermini --build
```

Исходники — `components/flint/board/models/flint_spark/s3_supermini/`. Селектор создаёт в каталоге платы `device_config.h`, `device_display.h`, `device.cmake`; сгенерированные файлы игнорируются Git. [Общая архитектура](../../platform/readme), [карта разделов](CONFIGURATION.md).
