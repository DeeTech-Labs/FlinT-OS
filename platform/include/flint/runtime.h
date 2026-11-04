#pragma once
#include "permissions.h"
#include "resources.h"
#include "storage.h"
#include <map>
namespace flint {
struct Event {
    uint32_t type{};
    uint64_t value{}, generation{};
};
class EventBus {
    std::array<Event, 32> queue_{};
    size_t head_{}, size_{};

public:
    Error post(Event e) {
        if (size_ == queue_.size())
            return Error::quota_exceeded;
        queue_[(head_ + size_) % queue_.size()] = e;
        ++size_;
        return Error::ok;
    }
    bool poll(Event &e) {
        if (!size_)
            return false;
        e = queue_[head_];
        head_ = (head_ + 1) % queue_.size();
        --size_;
        return true;
    }
    void clear() {
        head_ = size_ = 0;
    }
};
using Values = std::map<std::string, std::string>;
class Runtime {
    File file_;
    Package package_;
    Manifest manifest_;
    ComponentInfo component_;
    AppLifecycle lifecycle_;
    FlintRender::Scene template_;
    ResourceManager resources_;
    uint64_t generation_{};

public:
    ~Runtime() {
        stop();
    }
    Error launch(const Installed &installed, std::string_view component, MemoryBudget available,
        uint64_t caps);
    Error stop();
    Error suspend();
    Error resume();
    Error render(const Values &values, FlintRender::Scene &scene);
    const Manifest &manifest() const {
        return manifest_;
    }
    const ComponentInfo &component() const {
        return component_;
    }
    State state() const {
        return lifecycle_.state;
    }
    uint64_t generation() const {
        return generation_;
    }
    ResourceManager &resources() {
        return resources_;
    }
};
Error loadValues(const Installed &package, const ComponentInfo &component, Values &values);
} // namespace flint
