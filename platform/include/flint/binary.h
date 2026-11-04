#pragma once
#include "package.h"
#include <string>
#include <vector>
namespace flint {
using Bytes = std::vector<uint8_t>;
struct Writer {
    Bytes data;
    void n(uint64_t v, unsigned bytes = 4) {
        for (unsigned i = 0; i < bytes; ++i)
            data.push_back(uint8_t(v >> (8 * i)));
    }
    void raw(std::span<const uint8_t> b) {
        data.insert(data.end(), b.begin(), b.end());
    }
    void text(std::string_view s) {
        n(s.size(), 2);
        raw({reinterpret_cast<const uint8_t *>(s.data()), s.size()});
    }
};
struct Reader {
    std::span<const uint8_t> data;
    size_t pos{};
    bool valid{true};
    uint64_t n(unsigned bytes = 4) {
        if (bytes > 8 || pos + bytes > data.size()) {
            valid = false;
            return 0;
        }
        uint64_t v = 0;
        for (unsigned i = 0; i < bytes; ++i)
            v |= uint64_t(data[pos++]) << (8 * i);
        return v;
    }
    std::string text(size_t max = 192) {
        auto size = n(2);
        if (!valid || size > max || size > data.size() - pos) {
            valid = false;
            return {};
        }
        std::string s(reinterpret_cast<const char *>(data.data() + pos), size);
        pos += size;
        if (s.find('\0') != std::string::npos)
            valid = false;
        return s;
    }
    bool done() const {
        return valid && pos == data.size();
    }
};
inline bool identifier(std::string_view id) {
    if (id.empty() || id.size() > 64 || id.front() == '.' || id.back() == '.')
        return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'))
            return false;
    return id.find("..") == std::string_view::npos;
}
inline uint32_t crc32(std::span<const uint8_t> bytes) {
    uint32_t crc = ~0u;
    for (auto b : bytes) {
        crc ^= b;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
} // namespace flint
