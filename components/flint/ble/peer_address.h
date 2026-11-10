#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace flint::ble {
struct PeerAddress {
    uint8_t type{};
    std::array<uint8_t, 6> bytes{};
};
// Existing pairing revoke syntax: address type and six bytes in NimBLE storage order.
inline bool parsePeerAddress(std::string_view text, PeerAddress &output) {
    if (text.size() != 14 || (text[0] != '0' && text[0] != '1') || text[1] != ':')
        return false;
    const auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    PeerAddress parsed;
    parsed.type = uint8_t(text[0] - '0');
    for (std::size_t i = 0; i < parsed.bytes.size(); ++i) {
        const auto high = hex(text[2 + 2 * i]), low = hex(text[3 + 2 * i]);
        if (high < 0 || low < 0)
            return false;
        parsed.bytes[i] = uint8_t(high * 16 + low);
    }
    output = parsed;
    return true;
}
} // namespace flint::ble
