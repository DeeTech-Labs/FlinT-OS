[English translation](docs/en/development/contributing.md)

# Вклад в FlinT OS

## Подготовка

Для первого изменения начните с [README](README.md) и сборки нужной подсистемы. [Архитектура](docs/ru/architecture/overview.md), [Companion](docs/ru/architecture/companion-protocol.md) и [changelog](CHANGELOG.md) нужны по контексту правки, не как обязательное вступительное чтение. Вопросы обсуждаются в [Discussions](https://github.com/DeeTech-Labs/FlinT-OS/discussions), воспроизводимые ошибки и функции — в [Issues](https://github.com/DeeTech-Labs/FlinT-OS/issues). [Маршруты поддержки](SUPPORT.md).

Требуются ESP-IDF 6.1.0, C++20, Python 3.11+ и Pillow для ресурсов и тестов. Сборка: `python3 tools/select_device.py --build`. Аппаратные профили — `config/devices/` и `src/modules/`; карта Flash — `config/partitions.csv`.

## Общий SDK и организация кода

Перед сборкой выполните `git submodule update --init --recursive`. `platform/` — закреплённый подмодуль IgniteSDK. Общие изменения вносятся в SDK, проверяются там, затем подключаются обновлением коммита подмодуля в OS и Flare. Экспорта или копирования исходников нет.

В SDK заводские приложения находятся в `system/`. Генерация из корня SDK: `python3 tools/generate_registry.py` и `python3 tools/build_system.py`. Не запускайте удалённые инструменты экспорта.

OS владеет аппаратными адаптерами и transport integration; общие Runtime/authorizer принадлежат SDK. QML/desktop controllers изменяются в Flare, если задача затрагивает этот consumer. Ввод-вывод должен быть асинхронным; проверяйте полные ответы, фрагментацию и границы. Возможности берутся из INFO, а не жёстко заданного имени модели. Не добавляйте текстовые обходы бинарного протокола. Изменения контрактов документируются и проверяются в обоих потребителях.

## Проверка

Команды из корня потребителя:

```sh
cmake -S platform -B platform/build/docs-sdk -DCMAKE_BUILD_TYPE=Debug
cmake --build platform/build/docs-sdk --config Debug
ctest --test-dir platform/build/docs-sdk -C Debug --output-on-failure
```

Для OS дополнительно `python3 -m unittest discover -s tests/tooling -v`; для Flare — CTest основной сборки и тесты каталогов локализации. Обычные тесты автономны; проверки физического USB/BLE запускаются отдельно и явно. Не добавляйте реальные учётные данные, каталоги сборки, аппаратные захваты и личные настройки Qt Creator.

## Изменения и оформление

Создавайте небольшие PR из актуальной `main`, используя ветки `feature/…`, `fix/…`, `chore/…`, `docs/…`; напрямую в `main` не отправляйте. Объясните поведение, совместимость и проверку. Отступы — четыре пробела без табуляции; стиль в `.editorconfig` и `.clang-format`. Сообщения коммитов — английские Conventional Commits в повелительной форме, например `fix(session): Reject incomplete status snapshots`.

Русский — канонический язык файловой документации. Обновляйте оригиналы и английские переводы с сохранением имён документов и рабочих ссылок. Исторические журналы и сторонние лицензии сохраняют исходный язык.

## Приложения и лицензии

Независимые Ignite создаются отдельными проектами общим компилятором, без нативной регистрации циферблатов в Core. Разрешения объявляются с точной областью; неподдерживаемые API не рекламируются. Автор сохраняет права на собственный вклад. Вклад предоставляется по лицензии соответствующего компонента; если прав недостаточно, сообщите до включения. Передача авторских прав и общее согласие на коммерческое перелицензирование не предполагаются.

[Карта лицензий](docs/ru/legal/licensing.md), [LICENSE](LICENSE), [NOTICE](NOTICE). Участие регулируется [правилами сообщества](CODE_OF_CONDUCT.md).

## Быстрая проверка своего изменения

Для небольшого изменения не требуется читать все документы: [карта подсистем](docs/ru/development/extending.md)
указывает owning files. Выполните `git diff --check` и `python tools/check_docs.py` для текста.
Все build/scratch artifacts только в owning `build/`/`temp/`; для отдельно собираемого
SDK — `platform/build/`, не корень OS. Не смешивайте compiler kits в одном cache.

| Правка | Минимальный дополнительный набор |
| --- | --- |
| Конфиг/генератор | `python -m unittest discover -s tests/tooling -v`, selected/unselected modules/dependencies |
| Драйвер/worker | `cmake -S tests/host -B build/host`, build/CTest; ESP32-S3 build, physical errors/timing отдельно |
| Core/SDK | owning SDK Lua/no-Lua tests; OS build и Flare consumer tests; exact SHA, не copy |
| Firmware/recovery/storage | build/size; failure paths и отдельно power-cut с защищёнными данными |
| Документация/пример | links, реальные команды/конфиг, RU/EN; явно отметить недоступную плату/ОС |

Host build: `cmake --build build/host --config Debug --parallel 2`,
`ctest --test-dir build/host -C Debug --output-on-failure`. Pillow нужен в выбранном
interpreter до configure. [Отдельная firmware сборка](docs/ru/getting-started/building.md).
Не выполняйте hardware smoke/flash на чужой плате без согласования: smoke меняет test packages.
В issue/PR указывайте electrical revision, active TOML, firmware/SDK SHA, команду и наблюдаемый
симптом; скрывайте private data/keys. Невыполненные heap/stack, power, Flash, USB/BLE и
sensor checks перечисляйте явно; host tests не закрывают аппаратную приёмку.
