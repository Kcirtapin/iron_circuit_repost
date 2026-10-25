#ifndef IRON_CIRCUIT_MODEL_PROGRAMCARD_H
#define IRON_CIRCUIT_MODEL_PROGRAMCARD_H

#include "util/Types.h"
#include <string>
#include <vector>

struct ProgramCard {
    std::string name;
    std::string description;
    util::MoveType moveType = util::MoveType::NONE;
    int range = 0;
    // Path: sequence of relative directions the robot follows when this card is played.
    // Directions are relative to the robot's facing at turn start.
    // Example: {FORWARD, FORWARD} = dash 2 tiles forward
    //          {FORWARD, FORWARD, FORWARD, RIGHT} = 3 forward + 1 right
    std::vector<util::RelativeDir> path;
};

#endif // IRON_CIRCUIT_MODEL_PROGRAMCARD_H
