#ifndef IRON_CIRCUIT_MODEL_ENEMYROBOT_H
#define IRON_CIRCUIT_MODEL_ENEMYROBOT_H

#include "model/Robot.h"
#include "model/Action.h"
#include <memory>
#include <string>

class AIBehavior;

class EnemyRobot : public Robot {
public:
    EnemyRobot(int id, util::Vector2i pos, std::weak_ptr<class EventBus> eventBus);
    ~EnemyRobot();
    void setBehavior(std::unique_ptr<AIBehavior> behavior);
    void takeTurn();
    void submitDrawRequests(class DrawQueue* queue) override;
    void submitDrawRequests(class DrawQueue* queue, int pixelOffsetX, int pixelOffsetY);
    const Action& getTelegraphedAction() const { return m_telegraphedAction; }
    void setRecentActionLabel(std::string text, int turnsToShow = 1);
    void clearRecentActionLabel();
    void decayRecentActionLabel();
    void setAwareOfPlayer(bool aware) { m_awareOfPlayer = aware; }
    bool isAwareOfPlayer() const { return m_awareOfPlayer; }
    void setArchetypeId(std::string id) { m_archetypeId = std::move(id); }
    const std::string& getArchetypeId() const { return m_archetypeId; }
    void setSpritePath(std::string path) { m_spritePath = std::move(path); }
    const std::string& getSpritePath() const { return m_spritePath; }
    void setDamageReductionPct(float pct) { m_damageReductionPct = pct; }
    float getDamageReductionPct() const { return m_damageReductionPct; }
    void setExplodeOnDeathDamage(int dmg) { m_explodeOnDeathDamage = dmg; }
    int getExplodeOnDeathDamage() const { return m_explodeOnDeathDamage; }
    void setDisableOnHitTurns(int turns) { m_disableOnHitTurns = turns; }
    int getDisableOnHitTurns() const { return m_disableOnHitTurns; }
    void setAttackDelayTurns(int turns) { m_attackDelayTurns = std::max(1, turns); }
    bool consumeAttackDelayReady();
    void setFirstHitMultiplier(float mult) { m_firstHitMultiplier = mult; }
    float consumeDamageMultiplierOnHit();
    void setFastMover(bool fast) { m_fastMover = fast; }
    bool isFastMover() const { return m_fastMover; }
    void setSummonerConfig(int intervalTurns, int maxMinions);
    bool consumeSummonReady();
    void onMinionSpawned() { ++m_spawnedMinions; }
    void setHpRegenPerTurn(int regen) { m_hpRegenPerTurn = std::max(0, regen); }
    std::string hoverDescription() const;
    void tickConditions() override;
    void takeDamage(int d) override;
    bool isDefeated() const override { return m_hp <= 0; }
private:
    std::unique_ptr<AIBehavior> m_behavior;
    Action m_telegraphedAction;
    std::string m_recentActionLabel;
    int m_recentActionTurns = 0;
    bool m_awareOfPlayer = false;
    std::string m_archetypeId;
    float m_damageReductionPct = 0.0f;
    int m_explodeOnDeathDamage = 0;
    int m_disableOnHitTurns = 0;
    int m_attackDelayTurns = 1;
    int m_attackDelayCounter = 0;
    float m_firstHitMultiplier = 1.0f;
    bool m_firstHitConsumed = false;
    bool m_fastMover = false;
    int m_summonIntervalTurns = 0;
    int m_summonCounter = 0;
    int m_maxMinions = 0;
    int m_spawnedMinions = 0;
    int m_hpRegenPerTurn = 0;
    std::string m_spritePath = "resource/image/robot_sprite_enemy_melee.png";
};

#endif // IRON_CIRCUIT_MODEL_ENEMYROBOT_H
