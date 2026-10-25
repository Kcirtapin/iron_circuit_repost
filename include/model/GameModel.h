#ifndef IRON_CIRCUIT_MODEL_GAMEMODEL_H
#define IRON_CIRCUIT_MODEL_GAMEMODEL_H

#include "model/Action.h"
#include "util/Types.h"
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class PlayerRobot;
class EnemyRobot;
class Level;
class Component;
class EventBus;

class GameModel {
public:
    GameModel();
    ~GameModel();

    void loadLevel(int levelIndex);
    Level* getLevel() { return m_level.get(); }
    PlayerRobot* getPlayer() { return m_player.get(); }
    util::GameState getState() const { return m_state; }
    const std::optional<PlayerAction>& getPendingPlayerAction() const { return m_pendingPlayerAction; }
    const std::vector<std::pair<int, Action>>& getPendingEnemyActions() const { return m_pendingEnemyActions; }
    int getCurrentLevel() const { return m_currentLevel; }

    void submitPlayerAction(struct PlayerAction action);
    void submitEnemyAction(int enemyId, struct Action action);
    void collectEnemyActions();
    bool allActionsReady() const;
    void executeTurn();
    void applyUpgrade(std::unique_ptr<Component> component, int slot);
    bool spawnEnemyMinion(util::Vector2i origin);
    void pushCombatLogEntry(std::string entry);
    void clearCombatLog();
    const std::deque<std::string>& getCombatLogEntries() const { return m_combatLogEntries; }
    std::shared_ptr<EventBus> getEventBus() const { return m_eventBus; }

private:
    std::unique_ptr<Level> m_level;
    std::unique_ptr<PlayerRobot> m_player;
    std::unique_ptr<class RuleManager> m_ruleManager;
    std::shared_ptr<EventBus> m_eventBus;
    std::optional<PlayerAction> m_pendingPlayerAction;
    std::vector<std::pair<int, Action>> m_pendingEnemyActions;
    int m_turnNumber = 0;
    util::GameState m_state = util::GameState::NONE;
    int m_currentLevel = 1;  // New member to track current level
    int m_nextEnemyId = 1;
    std::deque<std::string> m_combatLogEntries;
    static constexpr size_t kMaxCombatLogEntries = 10;
};

#endif // IRON_CIRCUIT_MODEL_GAMEMODEL_H
