#pragma once
#include "binary.h"
#include "graphics/scene.h"
namespace flint {
inline constexpr uint32_t MaxPackageBytes = 1024 * 1024, MaxManifestBytes = 4096;
enum class Role : uint8_t { none, clock, launcher, settings, theme, locale };
struct Declaration {
    std::string permission, scope;
};
struct ComponentInfo {
    std::string id;
    Component type{Component::app};
    Role role{};
    uint32_t section{};
};
struct Manifest {
    std::string id, name, publisher;
    uint32_t version{};
    MemoryBudget budget{};
    uint64_t capabilities{};
    std::vector<Declaration> permissions;
    std::vector<ComponentInfo> components;
};
Error loadManifest(Package &package, Manifest &manifest);
Error validateApplication(
    Package &package, Manifest &manifest, MemoryBudget available, uint64_t capabilities);
} // namespace flint
