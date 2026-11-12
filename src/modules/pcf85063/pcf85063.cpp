#include "pcf85063.h"
#include <array>

namespace flint::modules {
namespace {
int bcd(uint8_t value) {
    return (value & 15) <= 9 && (value >> 4) <= 9 ? (value >> 4) * 10 + (value & 15) : -1;
}
uint8_t encode(int value) {
    return uint8_t((value / 10) * 16 + value % 10);
}
bool valid(const std::tm &t) {
    if (t.tm_year < 100 || t.tm_year > 199 || t.tm_mon < 0 || t.tm_mon > 11 || t.tm_hour < 0 ||
        t.tm_hour > 23 || t.tm_min < 0 || t.tm_min > 59 || t.tm_sec < 0 || t.tm_sec > 59 ||
        t.tm_wday < 0 || t.tm_wday > 6)
        return false;
    constexpr int days[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int max = days[t.tm_mon] + (t.tm_mon == 1 && (t.tm_year + 1900) % 4 == 0);
    return t.tm_mday >= 1 && t.tm_mday <= max;
}
} // namespace
esp_err_t Pcf85063::begin(i2c_master_bus_handle_t bus, uint8_t address) {
    auto error = device_.begin(bus, address);
    uint8_t control{};
    if (error == ESP_OK)
        error = device_.read(0, &control, 1);
    // Preserve oscillator capacitance/calibration, reject test/12-hour mode. STOP is recoverable
    // through set().
    if (error == ESP_OK && (control & 0x82))
        error = ESP_ERR_INVALID_STATE;
    if (error != ESP_OK)
        device_.stop();
    return error;
}
esp_err_t Pcf85063::read(std::tm &utc) {
    utc = {};
    std::array<uint8_t, 7> raw{};
    uint8_t control{};
    auto error = device_.read(0, &control, 1);
    if (error != ESP_OK)
        return error;
    if (control & 0xa2)
        return ESP_ERR_INVALID_STATE;
    error = device_.read(4, raw.data(), raw.size());
    if (error != ESP_OK)
        return error;
    if (raw[0] & 0x80)
        return ESP_ERR_INVALID_RESPONSE; // Oscillator stop: time is untrusted.
    std::tm value{};
    value.tm_sec = bcd(raw[0] & 0x7f);
    value.tm_min = bcd(raw[1] & 0x7f);
    value.tm_hour = bcd(raw[2] & 0x3f);
    value.tm_mday = bcd(raw[3] & 0x3f);
    value.tm_wday = raw[4] & 7;
    value.tm_mon = bcd(raw[5] & 0x1f) - 1;
    value.tm_year = bcd(raw[6]) + 100;
    if (!valid(value))
        return ESP_ERR_INVALID_RESPONSE;
    utc = value;
    return ESP_OK;
}
esp_err_t Pcf85063::set(const std::tm &utc) {
    if (!valid(utc))
        return ESP_ERR_INVALID_ARG;
    // A failed update leaves STOP set: partially written time must never be trusted.
    uint8_t control{};
    auto error = device_.read(0, &control, 1);
    if (error != ESP_OK)
        return error;
    error = device_.write(0, (control | 0x20) & ~0x82);
    if (error != ESP_OK)
        return error;
    const uint8_t values[]{encode(utc.tm_sec), encode(utc.tm_min), encode(utc.tm_hour),
        encode(utc.tm_mday), uint8_t(utc.tm_wday), encode(utc.tm_mon + 1),
        encode(utc.tm_year - 100)};
    error = device_.write(4, values, sizeof(values));
    if (error != ESP_OK)
        return error;
    return device_.write(0, control & ~0xa2);
}
} // namespace flint::modules
