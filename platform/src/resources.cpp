#include <flint/resources.h>
namespace flint {
Error ResourceManager::view(uint32_t id, Package::View &result) const {
    if (!package_)
        return Error::invalid_state;
    for (const auto &cached : views_) {
        if (cached.info().id == id) {
            result = cached;
            return Error::ok;
        }
    }
    for (auto &cached : views_) {
        if (!cached.info().id) {
            auto error = package_->view(id, cached);
            if (error == Error::ok)
                result = cached;
            return error;
        }
    }
    return Error::quota_exceeded;
}
Error ResourceManager::find(uint32_t id, Entry &out) const {
    Package::View section;
    const auto error = view(id, section);
    if (error == Error::ok)
        out = section.info();
    return error;
}
Error ResourceManager::read(ResourceHandle handle, uint32_t offset, std::span<uint8_t> bytes) {
    if (handle.generation != generation_)
        return Error::stale_handle;
    Package::View section;
    const auto error = view(handle.section, section);
    return error == Error::ok ? section.read(offset, bytes) : error;
}
FlintRender::Glyph ResourceManager::glyph(unsigned font, uint32_t cp) {
    Entry e;
    if (find(font, e) != Error::ok || e.type != Section::resource || e.size < 12)
        return {};
    std::array<uint8_t, 24> b{};
    auto h = handle(font);
    if (read(h, 0, std::span(b).first(12)) != Error::ok || u32(b.data()) != 0x46474c46)
        return {};
    auto count = u16(b.data() + 4);
    if (!count || count > 4096 || uint32_t(count) * 24 > e.size - 12)
        return {};
    unsigned low = 0, high = count;
    while (low < high) {
        unsigned mid = (low + high) / 2;
        if (read(h, 12 + mid * 24, b) != Error::ok)
            return {};
        auto code = u32(b.data());
        if (code < cp)
            low = mid + 1;
        else if (code > cp)
            high = mid;
        else {
            auto width = u16(b.data() + 4), height = u16(b.data() + 6),
                 advance = u16(b.data() + 12);
            auto offset = u32(b.data() + 16), size = u32(b.data() + 20);
            if (width > 64 || height > 64 || advance > 128 || size != uint32_t(width) * height ||
                offset < 12 + count * 24u || offset > e.size || size > e.size - offset)
                return {};
            if (read(h, offset, std::span(glyph_).first(size)) != Error::ok)
                return {};
            return {int(width), int(height), int(advance), int(int16_t(u16(b.data() + 10))),
                int(int16_t(u16(b.data() + 8))), glyph_.data()};
        }
    }
    return {};
}
void ResourceManager::image(FlintRender::Canvas &canvas, const FlintRender::Op &op) {
    Entry e;
    if (find(op.extra, e) != Error::ok || e.type != Section::resource || e.size < 8)
        return;
    auto h = handle(op.extra);
    std::array<uint8_t, 640> row{};
    if (read(h, 0, std::span(row).first(8)) != Error::ok || u32(row.data()) != 0x4d494c46)
        return;
    unsigned w = u16(row.data() + 4), height = u16(row.data() + 6);
    if (w > 320 || height > 320 || w != unsigned(op.w) || height != unsigned(op.h) ||
        e.size != 8 + w * height * 2)
        return;
    for (unsigned y = 0; y < height; ++y) {
        if (read(h, 8 + y * w * 2, std::span(row).first(w * 2)) != Error::ok)
            return;
        for (unsigned x = 0; x < w; ++x)
            canvas.pixel(op.x + x, op.y + y, u16(row.data() + x * 2));
    }
}

} // namespace flint
