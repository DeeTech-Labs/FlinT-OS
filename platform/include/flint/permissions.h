#pragma once
#include "contracts.h"

namespace flint {
enum class Access : uint8_t {
    automatic,
    persistent,
    session,
    selected_resource,
    explicit_action,
    restricted
};
struct Permission {
    std::string_view id;
    Access access;
    uint64_t capability;
};
// Records are issued only by the trusted consent service. Never deserialize a grant
// supplied by an Ignite or accept its claimed publisher as a verified identity.
struct Principal {
    uint64_t app{}, publisher{}, generation{};
};
struct Grant {
    Principal principal{};
    std::string_view permission{}, scope{};
    uint64_t expires_at{}, session{}, action{};
    bool active{};
};
struct Call {
    Principal principal{};
    std::string_view scope{};
    uint64_t now{}, session{}, action{}, capabilities{};
    bool declared{}, foreground{}, system_identity{};
};
inline bool same(Principal a, Principal b) {
    return a.app == b.app && a.publisher == b.publisher && a.generation == b.generation;
}
// Lives in Core/Companion. Remote checks must pass independently on both peers.
inline Error authorize(Permission permission, const Call &call, std::span<Grant> grants) {
    if (!call.declared || !call.principal.app || !call.principal.publisher)
        return Error::permission_denied;
    if ((call.capabilities & permission.capability) != permission.capability)
        return Error::not_supported;
    if (permission.access == Access::restricted)
        return call.system_identity ? Error::ok : Error::permission_denied;
    if (permission.access == Access::automatic)
        return Error::ok;
    for (auto &grant : grants) {
        if (!grant.active || !same(grant.principal, call.principal) ||
            grant.permission != permission.id || grant.scope != call.scope ||
            (grant.expires_at && call.now >= grant.expires_at))
            continue;
        if (permission.access == Access::session &&
            (!call.session || grant.session != call.session))
            continue;
        if (permission.access == Access::selected_resource && call.scope.empty())
            continue;
        if (permission.access == Access::explicit_action) {
            if (!call.action || grant.action != call.action || !grant.expires_at)
                continue;
            grant.active =
                false; // Single-use authorization; serialize calls in the Core event loop.
        }
        return Error::ok;
    }
    return call.foreground ? Error::permission_denied : Error::user_interaction_required;
}
inline void revoke(Principal principal, std::span<Grant> grants) {
    for (auto &grant : grants)
        if (same(grant.principal, principal))
            grant.active = false;
}
} // namespace flint
