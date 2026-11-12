[Русский оригинал](../../ru/architecture/ble-lifecycle.md)

# BLE lifecycle

Persistent `ble_enabled` controls the radio independently of Companion consent. An absent value migrates to `1`; `0` disables advertising and connections. The board has no buttons/touch: re-enable through USB. Bond keys in NVS are not erased. Settings ACK confirms acceptance of persistent policy; `ble_state`, `ble_transition_active`, and `ble_lifecycle_error` describe the hardware transition.

The Core owner manages intent and session termination. Start/stop uses a temporary `ble_policy` task with a 4096-byte stack because vendor `nimble_port_stop` waits for the host task without a bounded timeout. This task cannot access Core/PackageManager. After the host leaves `nimble_port_run`, the worker deletes it through `nimble_port_freertos_deinit`, then deinitializes host/controller and frees the RX queue. No extra task remains after transition and no idle polling is added. Transition failures are diagnostic and are not retried indefinitely.

Each new start checks current internal free/largest against reserves; `CONFIG_FLINT_BLE_START_MEMORY_REQUIREMENT` defaults to 64 KiB beyond the common reserve. Repeated intents coalesce. Disabling invalidates the queue, callbacks and session generation. The BLE settings reply may finish before stop, with a two-second grace bound. Host/worker watermark retains the minimum across start/stop cycles.
