#ifndef IRON_CIRCUIT_EVENTS_IEVENTLISTENER_H
#define IRON_CIRCUIT_EVENTS_IEVENTLISTENER_H

#include "events/GameEvent.h"

class IEventListener {
public:
    virtual ~IEventListener() = default;
    virtual void onEvent(const GameEvent& event) = 0;
};

#endif // IRON_CIRCUIT_EVENTS_IEVENTLISTENER_H
