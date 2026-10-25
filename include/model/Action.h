#ifndef IRON_CIRCUIT_MODEL_ACTION_H
#define IRON_CIRCUIT_MODEL_ACTION_H

#include "util/Types.h"
#include "model/ProgramCard.h"
#include <optional>

struct Action {
    util::MoveType moveType = util::MoveType::NONE;
    int targetX = 0;
    int targetY = 0;
    int damage = 0;
};

struct PlayerAction {
    ProgramCard card;
    util::Direction direction = util::Direction::NONE;
    bool valid = false;
};

#endif // IRON_CIRCUIT_MODEL_ACTION_H
