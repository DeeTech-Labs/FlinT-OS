#pragma once
#include "graphics/renderer.h"
#include "manifest.h"
namespace flint {
struct ResourceHandle {
    uint32_t section{};
    uint64_t generation{};
};
class ResourceManager {
    Package *package_{};
    uint64_t generation_{};
    std::array<uint8_t, 4096> glyph_{};
    mutable std::array<Package::View, 32> views_{};
    Error view(uint32_t id, Package::View &result) const;

public:
    void bind(Package *package) {
        package_ = package;
        views_ = {};
        ++generation_;
    }
    ResourceHandle handle(uint32_t id) const {
        return {id, generation_};
    }
    Error read(ResourceHandle handle, uint32_t offset, std::span<uint8_t> bytes);
    Error find(uint32_t id, Entry &entry) const;
    void image(FlintRender::Canvas &canvas, const FlintRender::Op &op);
    FlintRender::Glyph glyph(unsigned font, uint32_t codepoint);
};
} // namespace flint
