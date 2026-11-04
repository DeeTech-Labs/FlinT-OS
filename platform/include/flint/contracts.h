#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace flint {
inline constexpr uint16_t ApiVersion = 1, PackageVersion = 1, FaceApiVersion = 1,
                          ProtocolVersion = 1;
// Development contract identity is independent of the nominal public version.
inline constexpr uint32_t ContractRevision = 0x20261104;
inline constexpr std::string_view DeviceProfile = "flint_spark_s3_supermini";
enum class Error : uint16_t {
    ok,
    invalid_argument,
    invalid_package,
    incompatible_contract,
    not_supported,
    permission_denied,
    user_interaction_required,
    source_unavailable,
    cancelled,
    quota_exceeded,
    resource_exhausted,
    io_error,
    integrity_failed,
    invalid_state,
    stale_handle,
    execution_limit
};
enum class Component : uint16_t {
    app = 1,
    watchface,
    theme,
    locale,
    resource,
    service,
    widget,
    complication
};
enum class Lifecycle : uint8_t {
    create,
    activate,
    deactivate,
    event,
    suspend,
    resume,
    destroy,
    low_memory
};
enum class State : uint8_t { empty, inactive, active, suspended };
struct MemoryBudget {
    uint32_t internal_bytes{}, external_bytes{}, instructions{};
};
struct Identity {
    uint16_t protocol{ProtocolVersion};
    uint32_t revision{ContractRevision};
    std::string_view build_id{};
    uint64_t capabilities{};
};
inline Error compatible(const Identity &peer, uint64_t required) {
    if (peer.protocol != ProtocolVersion || peer.revision != ContractRevision ||
        peer.build_id.empty())
        return Error::incompatible_contract;
    return (peer.capabilities & required) == required ? Error::ok : Error::not_supported;
}
// Separate budgets: PSRAM availability never increases the internal/DMA budget.
inline bool fits(MemoryBudget need, MemoryBudget available) {
    return need.internal_bytes <= available.internal_bytes &&
        need.external_bytes <= available.external_bytes &&
        need.instructions <= available.instructions;
}
struct AppLifecycle {
    State state{State::empty};
    Error dispatch(Lifecycle event) {
        switch (event) {
        case Lifecycle::create:
            if (state != State::empty)
                break;
            state = State::inactive;
            return Error::ok;
        case Lifecycle::activate:
            if (state != State::inactive)
                break;
            state = State::active;
            return Error::ok;
        case Lifecycle::deactivate:
            if (state != State::active)
                break;
            state = State::inactive;
            return Error::ok;
        case Lifecycle::suspend:
            if (state != State::inactive)
                break;
            state = State::suspended;
            return Error::ok;
        case Lifecycle::resume:
            if (state != State::suspended)
                break;
            state = State::inactive;
            return Error::ok;
        case Lifecycle::destroy:
            if (state == State::empty || state == State::active)
                break;
            state = State::empty;
            return Error::ok;
        case Lifecycle::event:
            return state == State::active ? Error::ok : Error::invalid_state;
        case Lifecycle::low_memory:
            return state != State::empty ? Error::ok : Error::invalid_state;
        }
        return Error::invalid_state;
    }
};
} // namespace flint
