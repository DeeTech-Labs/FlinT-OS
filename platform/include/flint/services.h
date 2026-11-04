#pragma once
// Generated from spec/services.json; do not edit.
#include "protocol.h"

namespace flint {
struct ServiceDescriptor {
    Method method;
    std::string_view name;
    bool mutates;
};

inline constexpr ServiceDescriptor ServiceRegistry[]{
    ServiceDescriptor{Method::hello, "hello", false},
    ServiceDescriptor{Method::info, "info", false},
    ServiceDescriptor{Method::packages, "packages", false},
    ServiceDescriptor{Method::install_begin, "install_begin", true},
    ServiceDescriptor{Method::install_chunk, "install_chunk", true},
    ServiceDescriptor{Method::install_commit, "install_commit", true},
    ServiceDescriptor{Method::remove, "remove", true},
    ServiceDescriptor{Method::launch, "launch", true},
    ServiceDescriptor{Method::scene, "scene", false},
    ServiceDescriptor{Method::time_set, "time_set", true},
    ServiceDescriptor{Method::role_set, "role_set", true},
    ServiceDescriptor{Method::settings_get, "settings_get", false},
    ServiceDescriptor{Method::settings_set, "settings_set", true},
    ServiceDescriptor{Method::permissions_get, "permissions_get", false},
    ServiceDescriptor{Method::permission_set, "permission_set", true},
    ServiceDescriptor{Method::metrics, "metrics", true},
    ServiceDescriptor{Method::pairing, "pairing", true},
    ServiceDescriptor{Method::rollback, "rollback", true},
    ServiceDescriptor{Method::recover, "recover", true},
    ServiceDescriptor{Method::package_read, "package_read", false},
};

static_assert(uint16_t(Method::hello) == 1);
static_assert(uint16_t(Method::info) == 2);
static_assert(uint16_t(Method::packages) == 3);
static_assert(uint16_t(Method::install_begin) == 4);
static_assert(uint16_t(Method::install_chunk) == 5);
static_assert(uint16_t(Method::install_commit) == 6);
static_assert(uint16_t(Method::remove) == 7);
static_assert(uint16_t(Method::launch) == 8);
static_assert(uint16_t(Method::scene) == 9);
static_assert(uint16_t(Method::time_set) == 10);
static_assert(uint16_t(Method::role_set) == 11);
static_assert(uint16_t(Method::settings_get) == 12);
static_assert(uint16_t(Method::settings_set) == 13);
static_assert(uint16_t(Method::permissions_get) == 14);
static_assert(uint16_t(Method::permission_set) == 15);
static_assert(uint16_t(Method::metrics) == 16);
static_assert(uint16_t(Method::pairing) == 17);
static_assert(uint16_t(Method::rollback) == 18);
static_assert(uint16_t(Method::recover) == 19);
static_assert(uint16_t(Method::package_read) == 20);
} // namespace flint
