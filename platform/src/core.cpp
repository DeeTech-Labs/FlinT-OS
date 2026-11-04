#include <cmath>
#include <ctime>
#include <flint/core.h>
#include <flint/os.h>
#include <flint/registry.h>
#include <flint/services.h>
#include <sys/stat.h>
namespace flint {
static Bytes valuesBytes(const Values &values) {
    Writer w;
    w.n(values.size(), 2);
    for (auto &[key, value] : values) {
        w.text(key);
        w.text(value);
    }
    return w.data;
}
static Values valuesRead(const std::string &path) {
    auto data = readText(path, 8192);
    Reader r{{reinterpret_cast<const uint8_t *>(data.data()), data.size()}};
    Values values;
    auto count = r.n(2);
    if (count > 128)
        return {};
    for (unsigned i = 0; i < count; ++i) {
        auto k = r.text(64), v = r.text(192);
        values[k] = v;
    }
    return r.done() ? values : Values{};
}
static uint64_t identity(std::string_view text) {
    Sha256 hash;
    hash.update({reinterpret_cast<const uint8_t *>(text.data()), text.size()});
    auto digest = hash.finish();
    Reader r{digest};
    return r.n(8);
}
static uint64_t generation(const Installed &p) {
    File file;
    if (!file.open(p.path))
        return 0;
    std::array<uint8_t, 64> h{};
    if (!file.source().read(&file, 0, h))
        return 0;
    Reader r{std::span(h).subspan(32)};
    return r.n(8);
}
Core::Core(std::string root, std::string factory, Platform platform)
    : platform_(std::move(platform)),
      packages_(std::move(root), std::move(factory), platform_.memory, platform_.capabilities) {}
Error Core::persistSettings() {
    return writeAtomic(packages_.root() + "/settings.bin", valuesBytes(settings_))
        ? Error::ok
        : Error::io_error;
}
Error Core::persistGrants() {
    Writer w;
    w.n(grants_.size(), 2);
    for (auto &g : grants_) {
        w.text(g.package);
        w.text(g.permission);
        w.text(g.scope);
        w.n(g.generation, 8);
        w.n(g.expires, 8);
        w.n(g.session, 8);
        w.n(g.action, 8);
        w.n(g.enabled, 1);
    }
    if (w.data.size() > 16384) {
        return Error::quota_exceeded;
    }
    return writeAtomic(packages_.root() + "/grants.bin", w.data) ? Error::ok : Error::io_error;
}
Error Core::revokePackage(std::string_view id) {
    const auto previous = grants_;
    for (auto &grant : grants_) {
        if (grant.package == id) {
            grant.enabled = false;
        }
    }
    const auto result = persistGrants();
    if (result != Error::ok) {
        grants_ = previous;
    }
    haveMetrics_ = false;
    return result;
}
void Core::refreshProviders(std::string_view id, bool removed) {
    for (const auto role : {Role::theme, Role::locale}) {
        const auto key = "role" + std::to_string(unsigned(role));
        const auto selected = settings_.find(key);
        if (selected == settings_.end() || !selected->second.starts_with(std::string(id) + "/")) {
            continue;
        }
        if (removed) {
            settings_.erase(key);
            persistSettings();
        }
        activateRole(role);
    }
}
Error Core::begin() {
    auto error = packages_.begin();
    if (error != Error::ok)
        return error;
    base_ = platform_.monotonic();
    settings_ = valuesRead(packages_.root() + "/settings.bin");
    if (!settings_.contains("utc_offset")) {
        settings_["utc_offset"] = "180";
    }
    auto epoch = readText(packages_.root() + "/epoch", 24);
    if (!epoch.empty()) {
        char *tail{};
        auto value = strtoull(epoch.c_str(), &tail, 10);
        if (tail && !*tail && value >= 1700000000 && value <= 4102444800ULL)
            epoch_ = value;
    }
    auto data = readText(packages_.root() + "/grants.bin", 16384);
    Reader r{{reinterpret_cast<const uint8_t *>(data.data()), data.size()}};
    auto count = r.n(2);
    if (count <= 128) {
        for (unsigned i = 0; i < count; ++i) {
            PermissionRecord g;
            g.package = r.text(64);
            g.permission = r.text(80);
            g.scope = r.text(192);
            g.generation = r.n(8);
            g.expires = r.n(8);
            g.session = r.n(8);
            g.action = r.n(8);
            g.enabled = r.n(1);
            if (!g.session && !g.action)
                grants_.push_back(std::move(g));
        }
        if (!r.done())
            grants_.clear();
    }
    activateRole(Role::theme);
    activateRole(Role::locale);
    return activateRole(Role::clock);
}
uint64_t Core::now() const {
    return epoch_ ? epoch_ + (platform_.monotonic() - base_) / 1000 : 0;
}
Error Core::activateRole(Role role, std::string_view selection) {
    const auto key = "role" + std::to_string(unsigned(role));
    std::string selected(selection);
    if (selected.empty()) {
        auto i = settings_.find(key);
        if (i != settings_.end())
            selected = i->second;
    }
    Error result = Error::source_unavailable;
    for (unsigned pass = 0; pass < (selection.empty() ? 2u : 1u); ++pass) {
        packages_.visit([&](const Installed &installed) {
            for (const auto &component : installed.manifest.components) {
                const auto full = installed.manifest.id + "/" + component.id;
                const bool choose = pass == 0 ? !selected.empty() && full == selected
                                              : installed.factory && component.role == role;
                if (!choose) {
                    continue;
                }
                if (role == Role::theme) {
                    result = loadValues(installed, component, theme_);
                } else if (role == Role::locale) {
                    result = loadValues(installed, component, locale_);
                } else {
                    result = runtime_.launch(
                        installed, component.id, platform_.memory, platform_.capabilities);
                }
                if (result == Error::ok) {
                    if (!selection.empty()) {
                        const auto previous = settings_;
                        settings_[key] = full;
                        result = persistSettings();
                        if (result != Error::ok) {
                            settings_ = previous;
                        }
                    }
                    return false;
                }
            }
            return true;
        });
        if (result == Error::ok) {
            return result;
        }
    }
    return result;
}

Error Core::launch(std::string_view id, std::string_view component) {
    Installed p;
    auto error = packages_.locate(id, p);
    if (error != Error::ok)
        return error;
    auto result = runtime_.launch(p, component, platform_.memory, platform_.capabilities);
    if (result == Error::ok) {
        haveMetrics_ = false;
        events_.post({3, 0, runtime_.generation()});
    }
    return result;
}
bool Core::canReceiveMetrics() {
    const auto &m = runtime_.manifest();
    bool declared = false;
    for (auto &d : m.permissions)
        if (d.permission == "companion.pc.metrics" && d.scope == "cpu,ram")
            declared = true;
    if (!declared)
        return false;
    Installed installed;
    if (packages_.locate(m.id, installed) != Error::ok)
        return false;
    std::vector<Grant> grants;
    for (auto &g : grants_)
        if (g.package == m.id)
            grants.push_back({{identity(m.id), identity(m.publisher), g.generation}, g.permission,
                g.scope, g.expires, g.session, g.action, g.enabled});
    Call call{{identity(m.id), identity(m.publisher), generation(installed)}, "cpu,ram",
        platform_.monotonic(), 0, 0, platform_.capabilities, true, true, installed.factory};
    return authorize(*permission("companion.pc.metrics"), call, grants) == Error::ok;
}
bool Core::tick() {
    const auto minute = now() / 60;
    if (minute != renderedMinute_) {
        renderedMinute_ = minute;
        events_.post({1, minute, runtime_.generation()});
    }
    if (haveMetrics_ && platform_.monotonic() - metricAt_ >= 10000) {
        haveMetrics_ = false;
        events_.post({2, 0, runtime_.generation()});
    }
    bool dirty = false;
    Event event;
    while (events_.poll(event))
        if (event.generation == runtime_.generation()) {
            dirty = true;
        }
    return dirty;
}
Values Core::bindings() {
    Values values;
    auto epoch = now();
    int offset = 0;
    auto setting = settings_.find("utc_offset");
    if (setting != settings_.end()) {
        char *end = nullptr;
        const auto parsed = strtol(setting->second.c_str(), &end, 10);
        if (end && *end == 0 && parsed >= -720 && parsed <= 840) {
            offset = static_cast<int>(parsed);
        }
    }
    time_t local = epoch + offset * 60;
    struct tm tm{};
    gmtime_r(&local, &tm);
    char time[32], date[32];
    strftime(time, sizeof(time), "%H:%M", &tm);
    strftime(date, sizeof(date), "%d.%m.%Y", &tm);
    values["time.read"] = epoch ? time : "--:--";
    values["time.date"] = epoch ? date : "Sync time in Flare";
    values["system.info"] = std::string(DeviceProfile);
    for (auto &[key, value] : locale_)
        values["locale:" + key] = value;
    for (auto &[key, value] : settings_)
        values["settings:" + key] = value;
    std::string launcher;
    packages_.visit([&](const Installed &installed) {
        for (const auto &component : installed.manifest.components) {
            if (component.type == Component::app || component.type == Component::watchface) {
                if (launcher.size() + installed.manifest.name.size() >= 170) {
                    return false;
                }
                launcher += installed.manifest.name + "  ";
            }
        }
        return true;
    });
    values["packages.list"] = launcher;
    const auto &m = runtime_.manifest();
    if (canReceiveMetrics() && haveMetrics_ && platform_.monotonic() - metricAt_ < 10000) {
        char b[48];
        snprintf(b, sizeof(b), "CPU %.1f%%  RAM %.1f%%", cpu_, ram_);
        values["companion.pc.metrics"] = b;
    }
    for (auto it = values.begin(); it != values.end();) {
        auto key = it->first;
        std::string required = key.starts_with("locale:") ? "system.locale.read"
            : key.starts_with("settings:")                ? "system.settings.read"
            : key == "time.date"                          ? "time.read"
                                                          : key;
        bool declared = false;
        for (auto &declaration : m.permissions)
            if (declaration.permission == required)
                declared = true;
        if (!declared)
            it = values.erase(it);
        else
            ++it;
    }
    return values;
}
Error Core::scene(FlintRender::Scene &scene) {
    if (now() && platform_.monotonic() - lastSave_ >= 60000) {
        auto text = std::to_string(now());
        writeAtomic(packages_.root() + "/epoch",
            {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
        lastSave_ = platform_.monotonic();
    }
    auto error = runtime_.render(bindings(), scene);
    if (error != Error::ok) {
        const auto failed = runtime_.manifest().id + "/" + runtime_.component().id;
        runtime_.stop();
        const auto selected = settings_.find("role1");
        if (selected != settings_.end() && selected->second == failed) {
            settings_.erase(selected);
            persistSettings();
        }
        if (activateRole(Role::clock) != Error::ok) {
            return error;
        }
        error = runtime_.render(bindings(), scene);
        if (error != Error::ok) {
            return error;
        }
    }
    for (int i = 0; i < scene.count; ++i) {
        auto &color = scene.ops[i].color;
        const char *key = color == 0xfffd ? "background"
            : color == 0xfffc             ? "text"
            : color == 0xfffe             ? "accent"
                                          : nullptr;
        if (key) {
            auto it = theme_.find(key);
            if (it != theme_.end())
                color = strtoul(it->second.c_str(), nullptr, 16);
        }
    }
    return Error::ok;
}
void Core::paint(const FlintRender::Scene &scene, FlintRender::Canvas &canvas) {
    FlintRender::resourceContext = &runtime_.resources();
    FlintRender::resourceGlyph = [](void *context, unsigned font, uint32_t cp) {
        return static_cast<ResourceManager *>(context)->glyph(font, cp);
    };
    FlintRender::resourceImage = [](void *context, FlintRender::Canvas &c,
                                     const FlintRender::Op &op) {
        static_cast<ResourceManager *>(context)->image(c, op);
    };
    FlintRender::render(scene, canvas);
    FlintRender::resourceContext = nullptr;
    FlintRender::resourceGlyph = nullptr;
    FlintRender::resourceImage = nullptr;
}
void Core::disconnect(Session &session) {
    for (auto &g : grants_)
        if (g.session == session.id && g.session)
            g.enabled = false;
    session.compatible = false;
    haveMetrics_ = false;
    events_.clear();
    events_.post({4, 0, runtime_.generation()});
}
Frame Core::handle(const Frame &request, Session &session) {
    Frame reply{request.method, request.id, true, {}};
    Writer out;
    Reader in{request.payload};
    Error error = Error::ok;
    auto fail = [&](Error e) { error = e; };
    if (request.reply || !request.id)
        fail(Error::invalid_argument);
    else if (request.method == Method::hello) {
        auto protocol = in.n(2), revision = in.n();
        auto build = in.text(96);
        auto required = in.n(8);
        if (!in.done())
            fail(Error::invalid_argument);
        else {
            error = compatible(
                {uint16_t(protocol), uint32_t(revision), build, platform_.capabilities}, required);
            session.compatible = error == Error::ok;
            if (session.compatible) {
                out.n(ProtocolVersion, 2);
                out.n(ContractRevision);
                out.text(platform_.build_id);
                out.text(DeviceProfile);
                out.n(platform_.capabilities, 8);
            }
        }
    } else if (!session.authenticated)
        fail(Error::permission_denied);
    else if (!session.compatible)
        fail(Error::incompatible_contract);
    else
        switch (request.method) {
        case Method::info: {
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            out.text(DeviceProfile);
            out.text(platform_.build_id);
            out.n(platform_.memory.internal_bytes);
            out.n(platform_.memory.external_bytes);
            out.n(platform_.free_storage ? platform_.free_storage() : 0, 8);
            out.text(runtime_.manifest().id);
            out.text(runtime_.component().id);
            out.n(now(), 8);
            out.n(runtime_.generation(), 8);
            break;
        }
        case Method::packages: {
            const auto page = in.n();
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            Writer rows;
            uint32_t total = 0;
            unsigned count = 0;
            bool full = false;
            packages_.visit([&](const Installed &installed) {
                const auto index = total++;
                if (index < page || full) {
                    return true;
                }
                Writer row;
                row.text(installed.manifest.id);
                row.text(installed.manifest.name);
                row.text(installed.manifest.publisher);
                row.n(installed.manifest.version);
                row.n(installed.factory, 1);
                row.n(installed.manifest.components.size(), 2);
                for (const auto &component : installed.manifest.components) {
                    row.text(component.id);
                    row.n(uint16_t(component.type), 2);
                    row.n(uint8_t(component.role), 1);
                }
                if (rows.data.size() + row.data.size() > MaxPayload - 16) {
                    full = true;
                } else {
                    rows.raw(row.data);
                    ++count;
                }
                return true;
            });
            if (page > total) {
                fail(Error::invalid_argument);
                break;
            }
            out.n(total);
            out.n(count, 2);
            out.raw(rows.data);
            break;
        }
        case Method::install_begin: {
            auto total = in.n();
            auto digest = in.text(64);
            if (!in.done() || total < 64 || total > MaxPackageBytes || digest.size() != 64) {
                fail(Error::invalid_argument);
                break;
            }
            bool hex = true;
            for (char c : digest)
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                    hex = false;
            if (!hex) {
                fail(Error::invalid_argument);
                break;
            }
            auto meta = std::to_string(total) + ":" + digest;
            auto path = packages_.staging();
            auto previous = readText(path + ".meta", 100);
            struct stat st{};
            bool resume = previous == meta && stat(path.c_str(), &st) == 0 && st.st_size >= 0 &&
                uint64_t(st.st_size) <= total;
            if (!resume) {
                if (platform_.free_storage && platform_.free_storage() < total + 8192) {
                    fail(Error::quota_exceeded);
                    break;
                }
                FILE *f = fopen(path.c_str(), "wb");
                if (!f) {
                    fail(Error::io_error);
                    break;
                }
                fclose(f);
                if (!writeAtomic(path + ".meta",
                        {reinterpret_cast<const uint8_t *>(meta.data()), meta.size()})) {
                    fail(Error::io_error);
                    break;
                }
            }
            out.n(resume ? st.st_size : 0);
            break;
        }
        case Method::install_chunk: {
            auto offset = in.n();
            if (!in.valid || in.pos == in.data.size()) {
                fail(Error::invalid_argument);
                break;
            }
            auto path = packages_.staging();
            auto meta = readText(path + ".meta", 100);
            auto split = meta.find(':');
            if (split == std::string::npos) {
                fail(Error::invalid_state);
                break;
            }
            auto total = strtoul(meta.substr(0, split).c_str(), nullptr, 10);
            auto count = in.data.size() - in.pos;
            FILE *f = fopen(path.c_str(), "r+b");
            if (!f) {
                fail(Error::io_error);
                break;
            }
            fseek(f, 0, SEEK_END);
            auto size = ftell(f);
            if (size < 0 || offset > uint64_t(size) || offset + count > total) {
                fclose(f);
                fail(Error::invalid_argument);
                break;
            }
            if (offset < uint64_t(size)) {
                if (offset + count > uint64_t(size)) {
                    fclose(f);
                    fail(Error::invalid_argument);
                    break;
                }
                Bytes old(count);
                fseek(f, offset, SEEK_SET);
                if (fread(old.data(), 1, count, f) != count ||
                    !std::equal(old.begin(), old.end(), in.data.begin() + in.pos))
                    fail(Error::integrity_failed);
            } else if (fwrite(in.data.data() + in.pos, 1, count, f) != count || fflush(f) ||
                fsync(fileno(f)))
                fail(Error::io_error);
            fclose(f);
            if (error == Error::ok)
                out.n(std::max<uint64_t>(size, offset + count));
            break;
        }
        case Method::install_commit: {
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            auto path = packages_.staging();
            auto meta = readText(path + ".meta", 100);
            auto split = meta.find(':');
            File file;
            if (split == std::string::npos || !file.open(path) ||
                file.size != strtoul(meta.substr(0, split).c_str(), nullptr, 10)) {
                fail(Error::invalid_state);
                break;
            }
            Sha256 hash;
            std::array<uint8_t, 256> buffer{};
            for (uint32_t offset = 0; offset < file.size;) {
                auto count = std::min<uint32_t>(buffer.size(), file.size - offset);
                auto bytes = std::span(buffer).first(count);
                if (!file.source().read(&file, offset, bytes)) {
                    fail(Error::io_error);
                    break;
                }
                hash.update(bytes);
                offset += count;
            }
            file.close();
            std::string digest;
            const char *hex = "0123456789abcdef";
            for (auto b : hash.finish()) {
                digest += hex[b >> 4];
                digest += hex[b & 15];
            }
            if (error != Error::ok)
                break;
            if (digest != meta.substr(split + 1)) {
                fail(Error::integrity_failed);
                break;
            }
            Manifest candidate;
            error = packages_.inspect(path, candidate);
            if (error != Error::ok)
                break;
            error = revokePackage(candidate.id);
            if (error != Error::ok) {
                break;
            }
            bool active = runtime_.manifest().id == candidate.id;
            std::string component = runtime_.component().id, id;
            if (active)
                runtime_.stop();
            error = packages_.commit(path, id);
            if (error == Error::ok) {
                unlink((path + ".meta").c_str());
                refreshProviders(id, false);
                out.text(id);
            }
            if (active) {
                auto launchError = launch(candidate.id, component);
                if (launchError != Error::ok) {
                    packages_.rollback(candidate.id);
                    launch(candidate.id, component);
                    if (error == Error::ok)
                        error = launchError;
                }
            }
            break;
        }
        case Method::remove: {
            auto id = in.text(64);
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            Installed target;
            error = packages_.locate(id, target);
            if (error != Error::ok || target.factory) {
                if (target.factory)
                    error = Error::permission_denied;
                break;
            }
            error = revokePackage(id);
            if (error != Error::ok) {
                break;
            }
            bool active = runtime_.manifest().id == id;
            if (active)
                runtime_.stop();
            error = packages_.remove(id);
            if (active)
                activateRole(Role::clock);
            if (error == Error::ok) {
                refreshProviders(id, true);
            }
            break;
        }
        case Method::launch: {
            auto id = in.text(64), component = in.text(64);
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            error = launch(id, component);
            break;
        }
        case Method::scene: {
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            FlintRender::Scene scene;
            error = this->scene(scene);
            if (error == Error::ok) {
                std::array<uint8_t, FlintRender::MaxEncoded> bytes{};
                auto size = FlintRender::encode(scene, bytes.data(), bytes.size());
                out.raw(std::span(bytes).first(size));
            }
            break;
        }
        case Method::time_set: {
            auto epoch = in.n(8);
            if (!in.done() || epoch < 1700000000 || epoch > 4102444800ULL) {
                fail(Error::invalid_argument);
                break;
            }
            auto previous = epoch_;
            auto previousBase = base_;
            epoch_ = epoch;
            base_ = platform_.monotonic();
            auto text = std::to_string(epoch);
            if (!writeAtomic(packages_.root() + "/epoch",
                    {reinterpret_cast<const uint8_t *>(text.data()), text.size()})) {
                epoch_ = previous;
                base_ = previousBase;
                fail(Error::io_error);
            }
            break;
        }
        case Method::role_set: {
            auto role = Role(in.n(1));
            auto selection = in.text(130);
            if (!in.done() || role == Role::none || uint8_t(role) > 5) {
                fail(Error::invalid_argument);
                break;
            }
            error = activateRole(role, selection);
            break;
        }
        case Method::settings_get: {
            if (!in.done())
                fail(Error::invalid_argument);
            else
                out.raw(valuesBytes(settings_));
            break;
        }
        case Method::settings_set: {
            auto key = in.text(64), value = in.text(192);
            if (!in.done() || key != "utc_offset") {
                fail(Error::invalid_argument);
                break;
            }
            char *tail{};
            auto offset = strtol(value.c_str(), &tail, 10);
            if (value.empty() || !tail || *tail || offset < -720 || offset > 840) {
                fail(Error::invalid_argument);
                break;
            }
            auto old = settings_;
            settings_[key] = value;
            error = persistSettings();
            if (error != Error::ok)
                settings_ = std::move(old);
            break;
        }
        case Method::permissions_get: {
            auto id = in.text(64);
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            Installed p;
            error = packages_.locate(id, p);
            if (error != Error::ok)
                break;
            out.n(p.manifest.permissions.size(), 2);
            auto gen = generation(p);
            for (auto &d : p.manifest.permissions) {
                auto *spec = permission(d.permission);
                out.text(d.permission);
                out.text(d.scope);
                out.n(spec ? uint8_t(spec->access) : 255, 1);
                bool enabled = spec && spec->access == Access::automatic;
                for (auto &g : grants_)
                    if (g.package == id && g.permission == d.permission && g.scope == d.scope &&
                        g.generation == gen && g.enabled &&
                        (!g.session || g.session == session.id) &&
                        (!g.expires || platform_.monotonic() < g.expires))
                        enabled = true;
                out.n(enabled, 1);
            }
            break;
        }
        case Method::permission_set: {
            auto id = in.text(64), name = in.text(80), scope = in.text(192);
            bool enabled = in.n(1);
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            auto *spec = permission(name);
            if (!spec || spec->access == Access::restricted || spec->access == Access::automatic) {
                fail(Error::permission_denied);
                break;
            }
            Installed p;
            error = packages_.locate(id, p);
            if (error != Error::ok)
                break;
            bool declared = false;
            for (auto &d : p.manifest.permissions)
                if (d.permission == name && d.scope == scope)
                    declared = true;
            if (!declared) {
                fail(Error::permission_denied);
                break;
            }
            if (spec->access == Access::explicit_action) {
                fail(Error::not_supported);
                break;
            }
            auto old = grants_;
            auto it = std::find_if(grants_.begin(), grants_.end(), [&](auto &g) {
                return g.package == id && g.permission == name && g.scope == scope;
            });
            if (it == grants_.end()) {
                if (grants_.size() >= 128) {
                    fail(Error::quota_exceeded);
                    break;
                }
                grants_.push_back({});
                it = grants_.end() - 1;
            }
            *it = {id, name, scope, generation(p), 0,
                spec->access == Access::session ? session.id : 0, 0, enabled};
            error = persistGrants();
            if (error != Error::ok)
                grants_ = std::move(old);
            if (!enabled) {
                haveMetrics_ = false;
                events_.clear();
            }
            break;
        }
        case Method::metrics: {
            if (!canReceiveMetrics()) {
                fail(Error::permission_denied);
                break;
            }
            auto cpu = in.n(), ram = in.n();
            if (!in.done() || cpu > 10000 || ram > 10000) {
                fail(Error::invalid_argument);
                break;
            }
            cpu_ = cpu / 100.0;
            ram_ = ram / 100.0;
            metricAt_ = platform_.monotonic();
            haveMetrics_ = true;
            break;
        }
        case Method::pairing: {
            auto action = in.text(64);
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            if (!platform_.pairing) {
                fail(Error::not_supported);
                break;
            }
            out.raw(platform_.pairing(action));
            if (out.data.empty())
                fail(Error::permission_denied);
            break;
        }
        case Method::rollback: {
            auto id = in.text(64);
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            bool active = runtime_.manifest().id == id;
            auto component = runtime_.component().id;
            if (active)
                runtime_.stop();
            error = packages_.rollback(id);
            if (active) {
                if (launch(id, component) != Error::ok)
                    activateRole(Role::clock);
            }
            break;
        }
        case Method::package_read: {
            auto id = in.text(64);
            auto offset = in.n();
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            Installed p;
            error = packages_.locate(id, p);
            if (error != Error::ok)
                break;
            File file;
            if (!file.open(p.path)) {
                fail(Error::io_error);
                break;
            }
            if (offset > file.size) {
                fail(Error::invalid_argument);
                break;
            }
            auto count = std::min<uint32_t>(MaxPayload - 6, file.size - offset);
            Bytes data(count);
            if (!file.source().read(&file, offset, data)) {
                fail(Error::io_error);
                break;
            }
            out.n(file.size);
            out.raw(data);
            break;
        }
        case Method::recover: {
            if (!in.done()) {
                fail(Error::invalid_argument);
                break;
            }
            settings_.erase("role1");
            settings_.erase("role2");
            settings_.erase("role3");
            settings_.erase("role4");
            settings_.erase("role5");
            error = persistSettings();
            activateRole(Role::theme);
            activateRole(Role::locale);
            activateRole(Role::clock);
            break;
        }
        default:
            fail(Error::not_supported);
            break;
        }
    if (error == Error::ok && request.method != Method::scene && request.method != Method::info &&
        request.method != Method::packages && request.method != Method::package_read &&
        request.method != Method::permissions_get && request.method != Method::settings_get &&
        request.method != Method::hello && request.method != Method::install_chunk &&
        request.method != Method::install_begin)
        events_.post({4, 0, runtime_.generation()});
    Writer result;
    result.n(uint16_t(error), 2);
    if (error == Error::ok)
        result.raw(out.data);
    if (result.data.size() > MaxPayload) {
        result.data.clear();
        result.n(uint16_t(Error::quota_exceeded), 2);
    }
    reply.payload = std::move(result.data);
    return reply;
}
} // namespace flint
