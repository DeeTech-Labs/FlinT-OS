#pragma once

/**
 * @brief GPIO-защёлка питания SYS_EN и raw-состояния кнопок PWR / BOOT.
 * Включение: [modules.power_button], pins={hold, button, boot}. Hold active-high,
 * обе кнопки active-low. begin(hold, button, boot) сначала удерживает питание HIGH.
 * pressed()/bootPressed() читают уровень; debounce, click/long-press и UI-политика
 * находятся у вызывающего кода и не подменяются этим драйвером. Polling/ISR не создаётся.
 * В OS инициализация и raw diagnostics выполняются автоматически из Core.
 * releasePower() / deviceReleasePower() снимает удержание: вызывать только после
 * завершения записей Flash и остановки транспорта. При USB устройство может не погаснуть.
 * Ошибки проверять по ESP_OK; CHIP_PU/RST не является программной кнопкой этого модуля.
 * Деструктор намеренно не снимает питание. Нужна сверка GPIO с физической ревизией платы.
 */

#include "esp_err.h"
namespace flint::modules {
// Raw active-low button inputs. Policy, debounce and shutdown belong to the owner.
class PowerButton {
public:
    esp_err_t begin(int hold, int button, int boot);
    esp_err_t releasePower();
    bool pressed() const;
    bool bootPressed() const;
    bool ready() const {
        return hold_ >= 0;
    }

private:
    int hold_{-1}, button_{-1}, boot_{-1};
};
} // namespace flint::modules
