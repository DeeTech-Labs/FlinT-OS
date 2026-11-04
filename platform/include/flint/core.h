#pragma once
#include "protocol.h"
#include "runtime.h"
namespace flint {
inline constexpr uint64_t CapDeclarative = 1, CapResources = 2, CapPackages = 4, CapMetrics = 8,
                          CapUsb = 16, CapBle = 32;
struct Platform {
    std::string build_id;
    MemoryBudget memory{48 * 1024, 0, 4096};
    uint64_t capabilities{CapDeclarative | CapResources | CapPackages | CapMetrics};
    std::function<uint64_t()> monotonic;
    std::function<uint64_t()> free_storage;
    std::function<Bytes(std::string_view)> pairing;
};
struct Session {
    bool authenticated{}, compatible{};
    uint64_t id{};
};
struct PermissionRecord {
    std::string package, permission, scope;
    uint64_t generation{}, expires{}, session{}, action{};
    bool enabled{};
};
class Core {
    Platform platform_;
    PackageManager packages_;
    Runtime runtime_;
    Values settings_, theme_, locale_;
    EventBus events_;
    std::vector<PermissionRecord> grants_;
    uint64_t epoch_{}, base_{}, lastSave_{};
    double cpu_{}, ram_{};
    uint64_t metricAt_{};
    bool haveMetrics_{};
    uint64_t renderedMinute_{~uint64_t(0)};
    Error activateRole(Role role, std::string_view selection = {});
    Error persistSettings();
    Error persistGrants();
    Error revokePackage(std::string_view id);
    void refreshProviders(std::string_view id, bool removed);
    bool canReceiveMetrics();
    Values bindings();

public:
    Core(std::string storage, std::string factory, Platform platform);
    Error begin();
    Frame handle(const Frame &request, Session &session);
    Error launch(std::string_view id, std::string_view component);
    Error scene(FlintRender::Scene &scene);
    void paint(const FlintRender::Scene &scene, FlintRender::Canvas &canvas);
    void disconnect(Session &session);
    PackageManager &packages() {
        return packages_;
    }
    Runtime &runtime() {
        return runtime_;
    }
    bool tick();
    uint64_t now() const;
    const Platform &platform() const {
        return platform_;
    }
};
} // namespace flint
