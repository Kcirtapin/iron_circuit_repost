#ifndef IRON_CIRCUIT_EVENTS_GAMEEVENT_H
#define IRON_CIRCUIT_EVENTS_GAMEEVENT_H

#include "util/Types.h"

class Robot;

struct GameEvent {
    util::EventType type = util::EventType::TURN_START;
    Robot* source = nullptr;
    Robot* target = nullptr;
    int value = 0;
};

#endif // IRON_CIRCUIT_EVENTS_GAMEEVENT_H
