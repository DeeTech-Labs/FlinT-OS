[English translation](../../en/architecture/overview.md)

# Архитектура FlinT OS

`main/app_main.cpp` инициализирует NVS и запускает основной цикл. `src/core/application.cpp` владеет Core и связывает аппаратные адаптеры с переносимым SDK. USB, BLE и input передают работу владельцу Core. OS содержит аппаратные реализации, закреплённый `platform/` — общие контракты.

- [Дисплей и recovery](display.md).
- [Жизненный цикл BLE](ble-lifecycle.md).
- [Ввод и системный UI](input-and-system-ui.md).
- [Companion](companion-protocol.md).
- [Структура репозитория](../development/repository-layout.md).
- [Диагностика](../validation/performance.md).
- [IgniteSDK](../../../platform/docs/ru/ARCHITECTURE.md).

Настройки находятся в `config/`, реализации плат — в `src/board/models/`. Выбранные заголовки, версия и заводские пакеты создаются в каталоге сборки: `generated/` и `factory/`. Генерация OS не записывает эти артефакты в исходники.

Состояние незавершённой интеграции UI: [SDK integration](../development/sdk-integration.md).
