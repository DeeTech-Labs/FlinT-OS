#pragma once

/**
 * @brief Формат команды инициализации ST7789 для генерации из device.toml.
 * command — байт регистра; size — число значимых байтов data (0..14);
 * delayMs — пауза после команды. Селектор проверяет длины/типы до компиляции.
 * Использование: секция [modules.st7789.panel], массив init с command/data/delay_ms.
 * Пример записи TOML: { command = 0x11, data = [], delay_ms = 130 }.
 * generated/board/device_display.h формирует St7789Init; драйвер исполняет его из Core.
 * Сам по себе этот тип не включает модуль и не выполняет операций с оборудованием.
 */

#include <array>
#include <cstdint>

namespace flint::modules {
struct St7789InitCommand {
    uint8_t command, size;
    unsigned delayMs;
    std::array<uint8_t, 14> data;
};
} // namespace flint::modules
