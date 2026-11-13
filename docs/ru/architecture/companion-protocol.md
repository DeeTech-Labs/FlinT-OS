[Канонический wire и форматы SDK](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/ru/ARCHITECTURE.md) · [Generated Method/permission registry](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/ru/REGISTRY.md).

[English translation](../../en/architecture/companion-protocol.md)

# Протокол Companion v1

Бинарное кадрирование и типизированные методы определены в [общей спецификации](../../../platform/README.md) и `platform/spec/services.json`. Core, Flare и симулятор используют одинаковые кодеки.

Совместимость проверяется по версии протокола, ревизии контракта разработки, идентификатору сборки и необходимым возможностям. Одной версии 1 недостаточно. Текстового резервного протокола нет.

BLE требует шифрования, аутентификации и привязки. Открыть окно сопряжения и подтвердить числовое сравнение можно только через сохранённый локальный USB-канал. Разрешения устройства и локальная политика Flare независимы; для чувствительных данных нужны оба разрешения.

UUID сервиса, RX и TX: `e7f1000{1,2,3}-7a1e-4b5c-9d2f-46544c494e54`. Подтверждаемые фрагменты GATT по 20 байт переносят те же полные бинарные кадры, что USB.
