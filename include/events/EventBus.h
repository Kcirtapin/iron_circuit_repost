#ifndef IRON_CIRCUIT_EVENTS_EVENTBUS_H
#define IRON_CIRCUIT_EVENTS_EVENTBUS_H

#include "events/GameEvent.h"
#include "events/IEventListener.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>

class EventBus : public std::enable_shared_from_this<EventBus> {
public:
    static std::shared_ptr<EventBus> create() { return std::shared_ptr<EventBus>(new EventBus()); }

    void subscribe(util::EventType type, IEventListener* listener) {
        if (listener)
            m_listeners[static_cast<int>(type)].push_back(listener);
    }

    void unsubscribe(util::EventType type, IEventListener* listener) {
        auto it = m_listeners.find(static_cast<int>(type));
        if (it == m_listeners.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), listener), vec.end());
    }

    void publish(const GameEvent& ev) {
        auto it = m_listeners.find(static_cast<int>(ev.type));
        if (it == m_listeners.end()) return;
        // Copy list so callbacks can safely modify listeners
        auto copy = it->second;
        for (auto* l : copy)
            if (l) l->onEvent(ev);
    }

private:
    EventBus() = default;
    std::unordered_map<int, std::vector<IEventListener*>> m_listeners;
};

#endif // IRON_CIRCUIT_EVENTS_EVENTBUS_H
