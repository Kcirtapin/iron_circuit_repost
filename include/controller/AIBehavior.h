#ifndef IRON_CIRCUIT_CONTROLLER_AIBEHAVIOR_H
#define IRON_CIRCUIT_CONTROLLER_AIBEHAVIOR_H

#include <memory>
#include "model/Action.h"

class EnemyRobot;
class GameModel;

class AIBehavior {
public:
    virtual ~AIBehavior() = default;
    virtual Action tick() = 0;

    // Factory methods — return concrete AI subclasses
    static std::unique_ptr<AIBehavior> melee(EnemyRobot* owner, GameModel* model);
    static std::unique_ptr<AIBehavior> ranged(EnemyRobot* owner, GameModel* model, int range);
    static std::unique_ptr<AIBehavior> boss(EnemyRobot* owner, GameModel* model);

protected:
    AIBehavior(EnemyRobot* robot, GameModel* model)
        : m_robot(robot), m_model(model) {}

    EnemyRobot* m_robot;
    GameModel* m_model;
};

#endif // IRON_CIRCUIT_CONTROLLER_AIBEHAVIOR_H
