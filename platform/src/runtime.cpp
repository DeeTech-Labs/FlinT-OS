#include <flint/runtime.h>
namespace flint {
Error Runtime::launch(
    const Installed &installed, std::string_view id, MemoryBudget available, uint64_t caps) {
    File candidate;
    if (!candidate.open(installed.path))
        return Error::io_error;
    Package p;
    auto error = p.open(candidate.source());
    if (error != Error::ok)
        return error;
    Manifest m;
    error = validateApplication(p, m, available, caps);
    if (error != Error::ok)
        return error;
    bool canRender = false;
    for (auto &d : m.permissions)
        if (d.permission == "ui.render")
            canRender = true;
    if (!canRender)
        return Error::permission_denied;
    ComponentInfo component;
    bool found = false;
    for (auto &c : m.components)
        if (c.id == id) {
            component = c;
            found = true;
            break;
        }
    if (!found)
        return Error::source_unavailable;
    if (component.type != Component::app && component.type != Component::watchface &&
        component.type != Component::widget && component.type != Component::complication)
        return Error::not_supported;
    Entry e;
    bool ui = false;
    for (uint32_t i = 0; i < p.count(); ++i) {
        p.entry(i, e);
        if (e.id == component.section) {
            ui = e.type == Section::ui;
            break;
        }
    }
    if (!ui)
        return Error::not_supported;
    Bytes bytes(e.size);
    FlintRender::Scene scene;
    if (p.read(e, 0, bytes) != Error::ok || !FlintRender::decode(bytes.data(), bytes.size(), scene))
        return Error::invalid_package;
    bool ownResources = false;
    for (const auto &declaration : m.permissions) {
        ownResources = ownResources || declaration.permission == "ui.resources";
    }
    for (int index = 0; index < scene.count; ++index) {
        const auto &operation = scene.ops[index];
        if ((operation.kind == FlintRender::Text && operation.font >= 4) ||
            operation.kind == FlintRender::ResourceImage) {
            if (!ownResources) {
                return Error::permission_denied;
            }
            const uint32_t resource =
                operation.kind == FlintRender::ResourceImage ? operation.extra : operation.font;
            bool foundResource = false;
            for (uint32_t index = 0; index < p.count(); ++index) {
                Entry entry;
                if (p.entry(index, entry) == Error::ok && entry.id == resource &&
                    entry.type == Section::resource) {
                    foundResource = true;
                    break;
                }
            }
            if (!foundResource) {
                return Error::invalid_package;
            }
        }
    }
    stop();
    if (!file_.open(installed.path))
        return Error::io_error;
    error = package_.open(file_.source());
    if (error != Error::ok) {
        file_.close();
        return error;
    }
    manifest_ = std::move(m);
    component_ = std::move(component);
    template_ = scene;
    resources_.bind(&package_);
    ++generation_;
    lifecycle_.dispatch(Lifecycle::create);
    return lifecycle_.dispatch(Lifecycle::activate);
}
Error Runtime::stop() {
    if (lifecycle_.state == State::empty)
        return Error::ok;
    if (lifecycle_.state == State::active)
        lifecycle_.dispatch(Lifecycle::deactivate);
    lifecycle_.dispatch(Lifecycle::destroy);
    resources_.bind(nullptr);
    package_.close();
    file_.close();
    manifest_ = {};
    ++generation_;
    return Error::ok;
}
Error Runtime::suspend() {
    if (lifecycle_.state != State::active)
        return Error::invalid_state;
    lifecycle_.dispatch(Lifecycle::deactivate);
    return lifecycle_.dispatch(Lifecycle::suspend);
}
Error Runtime::resume() {
    auto error = lifecycle_.dispatch(Lifecycle::resume);
    return error == Error::ok ? lifecycle_.dispatch(Lifecycle::activate) : error;
}
Error Runtime::render(const Values &values, FlintRender::Scene &output) {
    if (lifecycle_.state != State::active)
        return Error::invalid_state;
    FlintRender::Scene scene;
    scene.width = template_.width;
    scene.height = template_.height;
    uint32_t instructions = 0;
    for (int i = 0; i < template_.count; ++i) {
        if (++instructions > manifest_.budget.instructions) {
            return Error::execution_limit;
        }
        auto op = template_.ops[i];
        auto *data = template_.data + op.offset;
        if (op.kind == FlintRender::Text) {
            std::string text(reinterpret_cast<const char *>(data), op.length);
            size_t start = 0;
            unsigned substitutions = 0;
            while ((start = text.find("${", start)) != std::string::npos) {
                if (++substitutions > 32 || ++instructions > manifest_.budget.instructions)
                    return Error::execution_limit;
                auto end = text.find('}', start);
                if (end == std::string::npos)
                    return Error::invalid_package;
                auto key = text.substr(start + 2, end - start - 2);
                std::string value;
                if (key.starts_with("resource:")) {
                    bool declared = false;
                    for (auto &d : manifest_.permissions)
                        if (d.permission == "ui.resources")
                            declared = true;
                    if (!declared)
                        return Error::permission_denied;
                    auto id = key.substr(9);
                    char *tail{};
                    auto number = strtoul(id.c_str(), &tail, 10);
                    if (!id.empty() && tail && !*tail && number <= UINT32_MAX) {
                        Entry entry;
                        if (resources_.find(number, entry) == Error::ok &&
                            entry.type == Section::resource && entry.size <= 192) {
                            Bytes b(entry.size);
                            if (resources_.read(resources_.handle(number), 0, b) == Error::ok)
                                value.assign(reinterpret_cast<const char *>(b.data()), b.size());
                        }
                    }
                } else {
                    auto it = values.find(key);
                    if (it != values.end())
                        value = it->second;
                    else
                        value = "—";
                }
                if (value.find('\0') != std::string::npos ||
                    text.size() - (end + 1 - start) + value.size() > 192)
                    return Error::quota_exceeded;
                text.replace(start, end + 1 - start, value);
                start += value.size();
            }
            if (!scene.add(op, text.data(), text.size()))
                return Error::quota_exceeded;
        } else if (!scene.add(op, data, op.length))
            return Error::quota_exceeded;
    }
    output = scene;
    return Error::ok;
}
Error loadValues(const Installed &installed, const ComponentInfo &component, Values &values) {
    File file;
    if (!file.open(installed.path))
        return Error::io_error;
    Package p;
    auto error = p.open(file.source());
    if (error != Error::ok)
        return error;
    Entry e;
    bool found = false;
    for (uint32_t i = 0; i < p.count(); ++i) {
        p.entry(i, e);
        if (e.id == component.section) {
            found = true;
            break;
        }
    }
    if (!found || e.size > 4096)
        return Error::invalid_package;
    Bytes bytes(e.size);
    if (p.read(e, 0, bytes) != Error::ok)
        return Error::io_error;
    Reader r{bytes};
    auto count = r.n(2);
    if (count > 64)
        return Error::invalid_package;
    Values result;
    for (unsigned i = 0; i < count; ++i) {
        auto key = r.text(64), value = r.text(192);
        if (key.empty() || !result.emplace(key, value).second)
            return Error::invalid_package;
    }
    if (!r.done())
        return Error::invalid_package;
    values = std::move(result);
    return Error::ok;
}
} // namespace flint
