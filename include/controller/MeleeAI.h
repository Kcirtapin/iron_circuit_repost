#ifndef IRON_CIRCUIT_CONTROLLER_MELEEAI_H
#define IRON_CIRCUIT_CONTROLLER_MELEEAI_H

#include "controller/AIBehavior.h"

class MeleeAI : public AIBehavior {
public:
    MeleeAI(EnemyRobot* robot, GameModel* model);
    Action tick() override;
};

#endif // IRON_CIRCUIT_CONTROLLER_MELEEAI_H
