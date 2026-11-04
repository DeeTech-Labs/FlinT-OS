#pragma once
#include "manifest.h"
#include <cstdio>
#include <functional>
namespace flint {
struct File {
    FILE *handle{};
    uint32_t size{};
    File() = default;
    File(const File &) = delete;
    File &operator=(const File &) = delete;
    ~File() {
        close();
    }
    bool open(const std::string &path);
    void close();
    Source source();
};
struct Installed {
    Manifest manifest;
    std::string path;
    bool factory{};
};
class PackageManager {
    std::string root_, factory_;
    MemoryBudget available_;
    uint64_t capabilities_;

public:
    PackageManager(
        std::string root, std::string factory, MemoryBudget available, uint64_t capabilities)
        : root_(std::move(root)), factory_(std::move(factory)), available_(available),
          capabilities_(capabilities) {}
    Error begin();
    void visit(const std::function<bool(const Installed &)> &visitor) const;
    std::vector<Installed> list() const;
    Error inspect(const std::string &path, Manifest &manifest) const;
    Error locate(std::string_view id, Installed &result) const;
    Error commit(const std::string &staging, std::string &installedId);
    Error remove(std::string_view id);
    Error rollback(std::string_view id);
    std::string staging() const {
        return root_ + "/incoming.part";
    }
    std::string root() const {
        return root_;
    }
};
bool writeAtomic(const std::string &path, std::span<const uint8_t> bytes);
std::string readText(const std::string &path, size_t max = 256);
} // namespace flint
