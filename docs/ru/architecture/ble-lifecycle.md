[English translation](../../en/architecture/ble-lifecycle.md)

# BLE lifecycle

Сохранённая `ble_enabled` управляет радио независимо от согласий Companion. Отсутствующее значение мигрирует в `1`; `0` выключает advertising и соединения. Плата не имеет кнопок/touch: включение после выключения выполняется через USB. Bond keys в NVS не стираются. Settings ACK означает принятие persistent политики; actual `ble_state`, `ble_transition_active` и `ble_lifecycle_error` описывают аппаратный переход.

Владелец Core управляет намерением и завершением сессий. Start/stop выполняется временной задачей `ble_policy` со стеком 4096 байт, поскольку vendor `nimble_port_stop` ожидает host task без ограниченного timeout; Core/PackageManager этой задаче недоступны. После выхода host из `nimble_port_run` worker удаляет его через `nimble_port_freertos_deinit`, затем деинициализирует controller/host и освобождает RX queue. Новые задачи не остаются после перехода; обычное idle-обслуживание не добавлено. Ошибки перехода видны в диагностике и автоматически бесконечно не повторяются.

Перед новым стартом проверяются текущие internal free/largest с резервом; `CONFIG_FLINT_BLE_START_MEMORY_REQUIREMENT` по умолчанию 64 KiB сверх общего резерва. Повторные намерения объединяются. При выключении очередь, callbacks и сессия теряют поколение; ответ по BLE может завершиться до stop, ожидание ограничено двумя секундами. Watermark host/worker сохраняет минимум между start/stop циклами.
