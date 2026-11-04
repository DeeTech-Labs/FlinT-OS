#pragma once
#include "contracts.h"
#include "sha256.h"
#include <algorithm>

namespace flint {
// Wire layout is decoded explicitly; never reinterpret bytes as a packed C++ struct.
inline constexpr std::array<uint8_t, 8> PackageMagic{'F', 'L', 'I', 'G', 'N', 'I', 'T', 0};
inline constexpr uint32_t HeaderSize = 64, EntrySize = 24, MaxEntries = 1024;
enum class Section : uint16_t { manifest = 1, component, resource, ui, wasm, locale, signature };
struct Entry {
    uint32_t id{}, offset{}, size{};
    Section type{};
    uint16_t codec{};
    uint32_t flags{};
};
using ReadAt = bool (*)(void *, uint32_t, std::span<uint8_t>);
struct Source {
    void *context{};
    uint32_t size{};
    ReadAt read{};
};
inline uint16_t u16(const uint8_t *p) {
    return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}
inline uint32_t u32(const uint8_t *p) {
    return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16;
}
class Package {
    Source source_{};
    uint32_t count_{};
    bool valid_{};
    uint64_t generation_{};

public:
    class View {
        friend class Package;
        const Package *owner_{};
        uint64_t generation_{};
        Entry entry_{};

    public:
        Entry info() const {
            return entry_;
        }
        Error read(uint32_t offset, std::span<uint8_t> bytes) const {
            if (!owner_ || !owner_->valid_ || generation_ != owner_->generation_) {
                return Error::stale_handle;
            }
            if (offset > entry_.size || bytes.size() > entry_.size - offset) {
                return Error::invalid_argument;
            }
            return owner_->source_.read(owner_->source_.context, entry_.offset + offset, bytes)
                ? Error::ok
                : Error::io_error;
        }
    };
    void close() {
        valid_ = false;
        ++generation_;
    }
    Error view(uint32_t id, View &result) const {
        if (!valid_) {
            return Error::invalid_state;
        }
        for (uint32_t index = 0; index < count_; ++index) {
            Entry entry;
            auto error = this->entry(index, entry);
            if (error != Error::ok)
                return error;
            if (entry.id == id) {
                result.owner_ = this;
                result.generation_ = generation_;
                result.entry_ = entry;
                return Error::ok;
            }
        }
        return Error::source_unavailable;
    }
    uint32_t count() const {
        return valid_ ? count_ : 0;
    }
    Error entry(uint32_t index, Entry &out) const {
        if (!valid_ || index >= count_)
            return Error::invalid_argument;
        std::array<uint8_t, EntrySize> b{};
        if (!source_.read(source_.context, HeaderSize + index * EntrySize, b))
            return Error::io_error;
        out = {u32(b.data()), u32(b.data() + 8), u32(b.data() + 12), Section(u16(b.data() + 4)),
            u16(b.data() + 6), u32(b.data() + 16)};
        if (u32(b.data() + 20) != 0)
            return Error::invalid_package;
        return Error::ok;
    }
    Error open(Source source) {
        ++generation_;
        valid_ = false;
        count_ = 0;
        source_ = source;
        if (!source.read || source.size < HeaderSize)
            return Error::invalid_package;
        std::array<uint8_t, HeaderSize> h{};
        if (!source.read(source.context, 0, h))
            return Error::io_error;
        if (!std::equal(PackageMagic.begin(), PackageMagic.end(), h.begin()) ||
            u16(h.data() + 8) != PackageVersion || u16(h.data() + 10) != HeaderSize ||
            u32(h.data() + 16) != source.size || u32(h.data() + 24) != HeaderSize ||
            u32(h.data() + 28) != 0)
            return Error::invalid_package;
        if (u32(h.data() + 12) != ContractRevision)
            return Error::incompatible_contract;
        count_ = u32(h.data() + 20);
        if (!count_ || count_ > MaxEntries || count_ > (source.size - HeaderSize) / EntrySize)
            return Error::invalid_package;
        valid_ = true;
        uint32_t end = HeaderSize + count_ * EntrySize, previous_id = 0, manifests = 0;
        for (uint32_t i = 0; i < count_; ++i) {
            Entry e;
            auto result = entry(i, e);
            if (result != Error::ok) {
                valid_ = false;
                return result;
            }
            auto type = uint16_t(e.type);
            // Entries sorted by ID and offset. No aliasing or overlaps, including zero-sized
            // sections.
            if (e.id <= previous_id || e.offset < end || e.offset > source.size || !e.size ||
                e.size > source.size - e.offset || type < 1 || type > 7 || e.codec != 0 ||
                e.flags != 0) {
                valid_ = false;
                return Error::invalid_package;
            }
            previous_id = e.id;
            end = e.offset + e.size;
            manifests += e.type == Section::manifest;
        }
        if (manifests != 1 || end != source.size) {
            valid_ = false;
            return Error::invalid_package;
        }
        Sha256 hash;
        const std::array<uint8_t, 32> zero{};
        hash.update(std::span(h).first(32));
        hash.update(zero);
        std::array<uint8_t, 256> chunk{};
        for (uint32_t offset = HeaderSize; offset < source.size;) {
            auto length = std::min<uint32_t>(chunk.size(), source.size - offset);
            auto part = std::span(chunk).first(length);
            if (!source.read(source.context, offset, part)) {
                valid_ = false;
                return Error::io_error;
            }
            hash.update(part);
            offset += length;
        }
        auto digest = hash.finish();
        if (!std::equal(digest.begin(), digest.end(), h.begin() + 32)) {
            valid_ = false;
            return Error::integrity_failed;
        }
        return Error::ok;
    }
    Error read(const Entry &entry, uint32_t offset, std::span<uint8_t> bytes) const {
        if (!valid_)
            return Error::invalid_state;
        // Resolve against the validated index, rather than trusting a caller-supplied offset.
        Entry actual;
        bool found = false;
        for (uint32_t i = 0; i < count_; ++i) {
            auto result = this->entry(i, actual);
            if (result != Error::ok)
                return result;
            if (actual.id == entry.id) {
                found = true;
                break;
            }
        }
        if (!found || offset > actual.size || bytes.size() > actual.size - offset)
            return Error::invalid_argument;
        return source_.read(source_.context, actual.offset + offset, bytes) ? Error::ok
                                                                            : Error::io_error;
    }
};
} // namespace flint
