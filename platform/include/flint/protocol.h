#pragma once
#include "binary.h"
#include <functional>
namespace flint {
inline constexpr size_t MaxPayload = 4096;
enum class Method : uint16_t {
    hello = 1,
    info,
    packages,
    install_begin,
    install_chunk,
    install_commit,
    remove,
    launch,
    scene,
    time_set,
    role_set,
    settings_get,
    settings_set,
    permissions_get,
    permission_set,
    metrics,
    pairing,
    rollback,
    recover,
    package_read
};
struct Frame {
    Method method{};
    uint32_t id{};
    bool reply{};
    Bytes payload;
};
inline Bytes encodeFrame(const Frame &frame) {
    if (frame.payload.size() > MaxPayload)
        return {};
    Writer w;
    w.n(0x50434c46);
    w.n(1, 1);
    w.n(frame.reply, 1);
    w.n(uint16_t(frame.method), 2);
    w.n(frame.id);
    w.n(frame.payload.size());
    w.raw(frame.payload);
    w.n(crc32(w.data));
    return w.data;
}
class FrameParser {
    Bytes bytes_;

public:
    void reset() {
        bytes_.clear();
    }
    void feed(std::span<const uint8_t> input, const std::function<void(Frame)> &receive) {
        for (auto byte : input) {
            bytes_.push_back(byte);
            while (bytes_.size() >= 4 && u32(bytes_.data()) != 0x50434c46)
                bytes_.erase(bytes_.begin());
            if (bytes_.size() < 16)
                continue;
            auto size = u32(bytes_.data() + 12);
            if (bytes_[4] != 1 || bytes_[5] > 1 || size > MaxPayload) {
                bytes_.erase(bytes_.begin());
                continue;
            }
            if (bytes_.size() < 20 + size)
                continue;
            if (crc32(std::span(bytes_).first(16 + size)) == u32(bytes_.data() + 16 + size)) {
                Frame f{Method(u16(bytes_.data() + 6)), u32(bytes_.data() + 8), bool(bytes_[5]),
                    Bytes(bytes_.begin() + 16, bytes_.begin() + 16 + size)};
                bytes_.clear();
                receive(std::move(f));
            } else
                bytes_.erase(bytes_.begin());
        }
    }
};
} // namespace flint
