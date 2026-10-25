#ifndef IRON_CIRCUIT_CONTROLLER_BOSSAI_H
#define IRON_CIRCUIT_CONTROLLER_BOSSAI_H

#include "controller/AIBehavior.h"

class BossAI : public AIBehavior {
public:
    BossAI(EnemyRobot* robot, GameModel* model);
    Action tick() override;
private:
    int m_turnCount = 0;
};

#endif // IRON_CIRCUIT_CONTROLLER_BOSSAI_H
