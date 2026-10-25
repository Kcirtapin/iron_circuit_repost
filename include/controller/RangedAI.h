#ifndef IRON_CIRCUIT_CONTROLLER_RANGEDAI_H
#define IRON_CIRCUIT_CONTROLLER_RANGEDAI_H

#include "controller/AIBehavior.h"

class RangedAI : public AIBehavior {
public:
    RangedAI(EnemyRobot* robot, GameModel* model, int range);
    Action tick() override;
private:
    int m_range;
};

#endif // IRON_CIRCUIT_CONTROLLER_RANGEDAI_H
