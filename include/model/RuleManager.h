#ifndef IRON_CIRCUIT_MODEL_RULEMANAGER_H
#define IRON_CIRCUIT_MODEL_RULEMANAGER_H

#include "Action.h"
#include "events/IEventListener.h"
#include <memory>
#include <optional>
#include <vector>

class GameModel;
class Level;
class PlayerRobot;
class EnemyRobot;

class RuleManager : public IEventListener {
public:
    RuleManager(GameModel* model, std::weak_ptr<class EventBus> eventBus);

    void onEvent(const struct GameEvent& event) override;
    void resolveSimultaneousActions(std::optional<struct PlayerAction> playerAction,
                                    std::vector<std::pair<int, Action>> enemyActions);
    void activateHazards();
    bool checkWinCondition() const;
    bool checkLossCondition() const;

private:
    void resolvePlayerAttack(const struct PlayerAction& action, PlayerRobot* player, Level* level);
    void resolvePlayerMove(const struct PlayerAction& action,   PlayerRobot* player, Level* level);
    void resolveEnemyAttack(const Action& action, EnemyRobot* enemy, PlayerRobot* player, Level* level);
    void resolveEnemyMove(const Action& action,   EnemyRobot* enemy, Level* level);

    GameModel* m_model;
    std::weak_ptr<class EventBus> m_eventBus;
};

#endif  // IRON_CIRCUIT_MODEL_RULEMANAGER_H
