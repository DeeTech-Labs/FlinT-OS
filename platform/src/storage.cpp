#include <flint/storage.h>
#ifndef _WIN32
#include <dirent.h>
#else
#include <direct.h>
#include <filesystem>
#define NOMINMAX
#include <windows.h>
#endif
#include <algorithm>
#include <cerrno>
#include <flint/os.h>
#include <map>
#include <set>
#include <sys/stat.h>
namespace flint {
static int replaceFile(const char *source, const char *target) {
#ifdef _WIN32
    return MoveFileExA(source, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(source, target);
#endif
}

bool File::open(const std::string &path) {
    close();
    struct stat st{};
    if (stat(path.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        uint64_t(st.st_size) > MaxPackageBytes)
        return false;
    size = st.st_size;
    handle = fopen(path.c_str(), "rb");
    return handle;
}
void File::close() {
    if (handle)
        fclose(handle);
    handle = nullptr;
    size = 0;
}
Source File::source() {
    return {this, size, [](void *context, uint32_t offset, std::span<uint8_t> out) {
                auto &f = *static_cast<File *>(context);
                return offset <= f.size && out.size() <= f.size - offset &&
                    fseek(f.handle, offset, SEEK_SET) == 0 &&
                    fread(out.data(), 1, out.size(), f.handle) == out.size();
            }};
}
bool writeAtomic(const std::string &path, std::span<const uint8_t> bytes) {
    const auto temporary = path + ".tmp";
    FILE *file = fopen(temporary.c_str(), "wb");
    if (!file)
        return false;
    bool ok = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    if (fflush(file) || fsync(fileno(file)))
        ok = false;
    if (fclose(file))
        ok = false;
    if (ok)
        ok = replaceFile(temporary.c_str(), path.c_str()) == 0;
    if (!ok)
        unlink(temporary.c_str());
    return ok;
}
std::string readText(const std::string &path, size_t max) {
    FILE *file = fopen(path.c_str(), "rb");
    if (!file)
        return {};
    std::string out(max + 1, '\0');
    auto size = fread(out.data(), 1, out.size(), file);
    fclose(file);
    if (size > max)
        return {};
    out.resize(size);
    return out;
}
static bool mkdirOne(const std::string &path) {
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(path.c_str(), 0700) == 0 || errno == EEXIST;
#endif
}
static std::vector<std::string> files(const std::string &path, std::string_view suffix) {
    std::vector<std::string> result;
#ifdef _WIN32
    std::error_code error;
    for (auto &e : std::filesystem::directory_iterator(path, error)) {
        auto name = e.path().filename().string();
        if (name.size() > suffix.size() && name.ends_with(suffix))
            result.push_back(name);
    }
#else
    auto *dir = opendir(path.c_str());
    if (!dir)
        return result;
    while (auto *e = readdir(dir)) {
        std::string name = e->d_name;
        if (name.size() > suffix.size() && name.ends_with(suffix))
            result.push_back(name);
    }
    closedir(dir);
#endif
    std::sort(result.begin(), result.end());
    return result;
}

static bool objectName(std::string_view name) {
    if (name.size() != 70 || !name.ends_with(".ignit"))
        return false;
    for (auto c : name.substr(0, 64))
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    return true;
}
static void collectObjects(const std::string &root) {
    std::set<std::string> used;
    for (auto &ref : files(root, ".ref")) {
        auto path = root + "/" + ref;
        used.insert(readText(path));
        used.insert(readText(path + ".previous"));
    }
    for (auto &object : files(root, ".ignit"))
        if (!used.contains(object))
            unlink((root + "/" + object).c_str());
    for (auto &temporary : files(root, ".tmp"))
        unlink((root + "/" + temporary).c_str());
}
Error PackageManager::begin() {
    if (!mkdirOne(root_))
        return Error::io_error;
    for (auto &ref : files(root_, ".ref")) {
        auto id = ref.substr(0, ref.size() - 4);
        if (!identifier(id))
            continue;
        auto path = root_ + "/" + ref;
        auto object = readText(path);
        Manifest current;
        if (!objectName(object) || inspect(root_ + "/" + object, current) != Error::ok ||
            current.id != id) {
            auto previous = readText(path + ".previous");
            Manifest backup;
            if (objectName(previous) && inspect(root_ + "/" + previous, backup) == Error::ok &&
                backup.id == id) {
                if (!writeAtomic(path,
                        {reinterpret_cast<const uint8_t *>(previous.data()), previous.size()}))
                    return Error::io_error;
            }
        }
    }
    collectObjects(root_);
    return Error::ok;
}
Error PackageManager::inspect(const std::string &path, Manifest &m) const {
    File f;
    if (!f.open(path))
        return Error::io_error;
    Package p;
    auto error = p.open(f.source());
    return error == Error::ok ? validateApplication(p, m, available_, capabilities_) : error;
}

static void scan(const std::string &path, std::string_view suffix,
    const std::function<bool(const std::string &)> &visitor) {
#ifdef _WIN32
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(path, error)) {
        const auto name = entry.path().filename().string();
        if (name.size() > suffix.size() && name.ends_with(suffix) && !visitor(name)) {
            break;
        }
    }
#else
    auto *directory = opendir(path.c_str());
    if (!directory) {
        return;
    }
    while (const auto *entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name.size() > suffix.size() && name.ends_with(suffix) && !visitor(name)) {
            break;
        }
    }
    closedir(directory);
#endif
}
void PackageManager::visit(const std::function<bool(const Installed &)> &visitor) const {
    bool proceed = true;
    scan(factory_, ".ignit", [&](const auto &name) {
        Manifest manifest;
        const auto path = factory_ + "/" + name;
        if (inspect(path, manifest) == Error::ok) {
            proceed = visitor({std::move(manifest), path, true});
        }
        return proceed;
    });
    if (!proceed) {
        return;
    }
    scan(root_, ".ref", [&](const auto &name) {
        const auto id = name.substr(0, name.size() - 4);
        if (!identifier(id)) {
            return true;
        }
        const auto object = readText(root_ + "/" + name);
        if (!objectName(object)) {
            return true;
        }
        Manifest manifest;
        const auto path = root_ + "/" + object;
        if (inspect(path, manifest) != Error::ok || manifest.id != id) {
            return true;
        }
        bool protectedId = false;
        scan(factory_, ".ignit", [&](const auto &factoryName) {
            Manifest factoryManifest;
            if (inspect(factory_ + "/" + factoryName, factoryManifest) == Error::ok &&
                factoryManifest.id == id) {
                protectedId = true;
                return false;
            }
            return true;
        });
        return protectedId || visitor({std::move(manifest), path, false});
    });
}
std::vector<Installed> PackageManager::list() const {
    std::vector<Installed> result;
    visit([&](const Installed &installed) {
        result.push_back(installed);
        return true;
    });
    return result;
}
Error PackageManager::locate(std::string_view id, Installed &result) const {
    if (!identifier(id)) {
        return Error::invalid_argument;
    }
    bool found = false;
    visit([&](const Installed &installed) {
        if (installed.manifest.id == id) {
            result = installed;
            found = true;
            return false;
        }
        return true;
    });
    return found ? Error::ok : Error::source_unavailable;
}
Error PackageManager::commit(const std::string &staging, std::string &id) {
    if (staging != this->staging())
        return Error::invalid_argument;
    Manifest m;
    auto error = inspect(staging, m);
    if (error != Error::ok)
        return error;
    Installed current;
    if (locate(m.id, current) == Error::ok) {
        // A claimed publisher string is not proof of identity. User packages cannot replace factory
        // identities.
        if (current.factory || current.manifest.publisher != m.publisher)
            return Error::permission_denied;
        if (m.version <= current.manifest.version)
            return Error::incompatible_contract;
    }
    File f;
    if (!f.open(staging))
        return Error::io_error;
    std::array<uint8_t, 64> h{};
    if (!f.source().read(&f, 0, h))
        return Error::io_error;
    f.close();
    const char *hex = "0123456789abcdef";
    std::string object;
    for (size_t i = 32; i < 64; ++i) {
        object += hex[h[i] >> 4];
        object += hex[h[i] & 15];
    }
    object += ".ignit";
    const auto ref = root_ + "/" + m.id + ".ref";
    auto previous = readText(ref);
    if (!previous.empty() &&
        !writeAtomic(ref + ".previous",
            {reinterpret_cast<const uint8_t *>(previous.data()), previous.size()}))
        return Error::io_error;
    if (replaceFile(staging.c_str(), (root_ + "/" + object).c_str()))
        return Error::io_error;
    if (!writeAtomic(ref, {reinterpret_cast<const uint8_t *>(object.data()), object.size()}))
        return Error::io_error;
    collectObjects(root_);
    id = m.id;
    return Error::ok;
}
Error PackageManager::remove(std::string_view id) {
    if (!identifier(id))
        return Error::invalid_argument;
    Installed p;
    auto error = locate(id, p);
    if (error != Error::ok)
        return error;
    if (p.factory)
        return Error::permission_denied;
    auto ref = root_ + "/" + std::string(id) + ".ref";
    if (unlink(ref.c_str()))
        return Error::io_error;
    unlink((ref + ".previous").c_str());
    // Immutable objects can be shared by current/previous refs. Delete only unreferenced objects.
    for (auto &object : files(root_, ".ignit")) {
        bool used = false;
        for (auto &name : files(root_, ".ref")) {
            auto path = root_ + "/" + name;
            if (readText(path) == object || readText(path + ".previous") == object) {
                used = true;
                break;
            }
        }
        if (!used)
            unlink((root_ + "/" + object).c_str());
    }
    return Error::ok;
}
Error PackageManager::rollback(std::string_view id) {
    if (!identifier(id))
        return Error::invalid_argument;
    auto ref = root_ + "/" + std::string(id) + ".ref";
    auto previous = readText(ref + ".previous");
    if (!objectName(previous))
        return Error::source_unavailable;
    Manifest m;
    auto error = inspect(root_ + "/" + previous, m);
    if (error != Error::ok || m.id != id)
        return Error::invalid_package;
    return writeAtomic(ref, {reinterpret_cast<const uint8_t *>(previous.data()), previous.size()})
        ? Error::ok
        : Error::io_error;
}
} // namespace flint
