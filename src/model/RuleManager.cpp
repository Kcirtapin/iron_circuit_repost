#include "model/RuleManager.h"
#include "model/GameModel.h"
#include "model/Level.h"
#include "model/PlayerRobot.h"
#include "model/EnemyRobot.h"
#include "model/Weapon.h"
#include "events/EventBus.h"
#include "events/GameEvent.h"
#include "util/Types.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace {
float computeHitChance(const Weapon* weapon, int damage) {
    if (!weapon) return 0.9f;
    const float rangePenalty = 0.045f * static_cast<float>(std::max(0, weapon->getRange() - 1));
    const float damagePenalty = 0.02f * static_cast<float>(std::max(0, damage - 5));
    const float chance = 0.94f - rangePenalty - damagePenalty;
    return std::clamp(chance, 0.35f, 0.95f);
}

bool rollHit(float chance) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) <= chance;
}

void applyKnockback(Level* level, Robot* source, EnemyRobot* target) {
    if (!level || !source || !target) return;
    const util::Vector2i s = source->getPos();
    const util::Vector2i t = target->getPos();
    util::Vector2i delta{t.x - s.x, t.y - s.y};
    if (delta.x != 0) delta.x = delta.x > 0 ? 1 : -1;
    if (delta.y != 0) delta.y = delta.y > 0 ? 1 : -1;
    util::Vector2i pushed{t.x + delta.x, t.y + delta.y};
    if (level->isInBounds(pushed) && level->getTileRef(pushed.x, pushed.y).isWalkable()) {
        level->moveRobot(target, pushed);
    }
}
}

RuleManager::RuleManager(GameModel* model, std::weak_ptr<EventBus> eventBus)
    : m_model(model), m_eventBus(std::move(eventBus)) {
    if (auto b = m_eventBus.lock())
        b->subscribe(util::EventType::ROBOT_DEFEATED, this);
}

void RuleManager::onEvent(const GameEvent& event) {
    if (event.type == util::EventType::ROBOT_DEFEATED) {
        // TODO: upgrade drop
    }
}

void RuleManager::resolveSimultaneousActions(
        std::optional<PlayerAction>             playerAction,
        std::vector<std::pair<int, Action>>     enemyActions) {

    Level*       level  = m_model->getLevel();
    PlayerRobot* player = m_model->getPlayer();
    if (!level || !player) return;

    // ── Player action ─────────────────────────────────────────────────────
    if (playerAction && playerAction->valid && !player->isDisabled()) {
        // 1) Set player facing
        if (playerAction->direction != util::Direction::NONE)
            player->setFacing(playerAction->direction);

        // 2) Resolve based on card type
        switch (playerAction->card.moveType) {
            case util::MoveType::MOVE:
                m_model->pushCombatLogEntry("P: " + playerAction->card.name);
                resolvePlayerMove(*playerAction, player, level);
                // Movement cards now fire the active weapon at end of path.
                resolvePlayerAttack(*playerAction, player, level);
                break;
            case util::MoveType::ATTACK:
                // Attack cards: first follow path (if any), then attack
                m_model->pushCombatLogEntry("P: " + playerAction->card.name);
                resolvePlayerMove(*playerAction, player, level);
                resolvePlayerAttack(*playerAction, player, level);
                break;
            case util::MoveType::SPECIAL:
                // TODO: per-card special handling
                break;
            default:
                break;
        }
    } else if (player && player->isDisabled()) {
        m_model->pushCombatLogEntry("P action disabled");
    }

    // ── Enemy actions ─────────────────────────────────────────────────────
    auto enemies = level->getEnemies();
    for (auto* e : enemies) {
        if (e) e->decayRecentActionLabel();
    }
    for (const auto& [id, action] : enemyActions) {
        EnemyRobot* enemy = nullptr;
        for (auto* e : enemies)
            if (e->getId() == id) { enemy = e; break; }
        if (!enemy) continue;

        if (enemy->consumeSummonReady()) {
            if (m_model->spawnEnemyMinion(enemy->getPos())) {
                enemy->onMinionSpawned();
                enemy->setRecentActionLabel("SUMMON", 1);
                m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " summoned minion");
            }
        }

        switch (action.moveType) {
            case util::MoveType::ATTACK:
                resolveEnemyAttack(action, enemy, player, level);
                break;
            case util::MoveType::MOVE:
                resolveEnemyMove(action, enemy, level);
                break;
            default:
                break;
        }
    }
}

// ── Player movement (path-based) ─────────────────────────────────────────────
void RuleManager::resolvePlayerMove(const PlayerAction& action,
                                     PlayerRobot* player, Level* level) {
    if (!action.valid || !player || !level) return;
    const auto& path = action.card.path;
    if (path.empty()) return;

    util::Direction facing = player->getFacing();
    for (const auto& relDir : path) {
        util::Direction worldDir = util::resolveRelative(facing, relDir);
        util::Vector2i offset = util::directionOffset(worldDir);
        util::Vector2i newPos = { player->getPos().x + offset.x,
                                  player->getPos().y + offset.y };
        if (level->isInBounds(newPos) && level->getTileRef(newPos.x, newPos.y).isWalkable()) {
            level->moveRobot(player, newPos);
            m_model->pushCombatLogEntry("P move -> (" + std::to_string(newPos.x) + "," + std::to_string(newPos.y) + ")");
        } else {
            if (player->tryBlinkMove(level, worldDir)) {
                m_model->pushCombatLogEntry("P blink -> (" + std::to_string(player->getPos().x) + "," + std::to_string(player->getPos().y) + ")");
                continue;
            }
            m_model->pushCombatLogEntry("P move blocked");
            break;  // stop movement on first blocked tile
        }
    }

}

// ── Player attack (weapon-dependent) ─────────────────────────────────────────
void RuleManager::resolvePlayerAttack(const PlayerAction& action,
                                       PlayerRobot* player, Level* level) {
    if (!action.valid || !player || !level) return;

    const Weapon* weapon = player->getActiveWeapon();
    if (weapon && weapon->isDisabled()) {
        m_model->pushCombatLogEntry("P weapon disabled");
        return;
    }
    int damage = weapon ? weapon->getDamage() : player->getAttackPower();
    if (player->isOverclockActive()) {
        damage = static_cast<int>(std::ceil(static_cast<float>(damage) * 1.5f));
    }
    m_model->pushCombatLogEntry("P atk " + std::to_string(damage));
    const float hitChance = computeHitChance(weapon, damage);
    const bool isShotgun = weapon && weapon->getName() == "Shotgun";
    const bool isRailgun = weapon && weapon->getName() == "Railgun";
    const bool isChainLightning = weapon && weapon->getName() == "Chain Lightning";
    const bool isTurretDeploy = weapon && weapon->getName() == "Turret Deploy";

    // Get attack tiles from weapon pattern (or default: 1 tile in front)
    std::vector<util::Vector2i> targetTiles;
    if (weapon) {
        targetTiles = weapon->getAttackTiles(player->getPos(), player->getFacing(), level);
    } else {
        // Default: single tile in front
        util::Vector2i offset = util::directionOffset(player->getFacing());
        targetTiles.push_back({player->getPos().x + offset.x,
                               player->getPos().y + offset.y});
    }

    // Apply damage to all enemies in the attack pattern
    if (auto b = m_eventBus.lock()) {
        b->publish(GameEvent{util::EventType::ATTACK_PERFORMED, player, nullptr, 0});
    }
    std::vector<EnemyRobot*> hitEnemies;
    for (const auto& tile : targetTiles) {
        if (!level->isInBounds(tile.x, tile.y)) continue;
        Robot* target = level->getRobotAt(tile);
        if (target && target != player) {
            auto* enemy = dynamic_cast<EnemyRobot*>(target);
            if (enemy && !enemy->isInvulnerable()) {
                if (!rollHit(hitChance)) {
                    enemy->setRecentActionLabel("MISS!", 1);
                    m_model->pushCombatLogEntry("P miss E" + std::to_string(enemy->getId()));
                    continue;
                }
                enemy->setRecentActionLabel("HIT!", 1);
                if (isRailgun) {
                    enemy->setHp(std::max(0, enemy->getHp() - damage));
                } else {
                    enemy->takeDamage(damage);
                }
                hitEnemies.push_back(enemy);
                const int heal = static_cast<int>(std::floor(static_cast<float>(damage) * player->getLifestealPct()));
                if (heal > 0) {
                    player->heal(heal);
                    m_model->pushCombatLogEntry("P lifesteal +" + std::to_string(heal));
                }
                m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " -" + std::to_string(damage) + " HP");
                if (isShotgun) {
                    applyKnockback(level, player, enemy);
                }
                if (auto b = m_eventBus.lock())
                    b->publish(GameEvent{util::EventType::DAMAGE_DEALT, player, target, damage});
                if (enemy->isDefeated()) {
                    m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " destroyed");
                    if (enemy->getExplodeOnDeathDamage() > 0) {
                        const util::Vector2i epos = enemy->getPos();
                        const std::vector<util::Vector2i> splash{
                            {epos.x + 1, epos.y}, {epos.x - 1, epos.y},
                            {epos.x, epos.y + 1}, {epos.x, epos.y - 1}
                        };
                        for (const auto& p : splash) {
                            if (!level->isInBounds(p)) continue;
                            Robot* splashTarget = level->getRobotAt(p);
                            if (splashTarget) {
                                splashTarget->takeDamage(enemy->getExplodeOnDeathDamage());
                            }
                        }
                        m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " exploded");
                    }
                    if (auto b = m_eventBus.lock())
                        b->publish(GameEvent{util::EventType::ROBOT_DEFEATED, player, target, 0});
                }

                // Heat gain on dealing damage
                const int heatGain = 1;
                player->addHeat(heatGain);
                m_model->pushCombatLogEntry("P gained " + std::to_string(heatGain) + " heat from dealing damage");
            }
            // Melee weapons stop at first hit; ranged hit all in line
            if (weapon && weapon->getWeaponType() == util::WeaponType::MELEE)
                break;
        }
    }

    if (isChainLightning && !hitEnemies.empty()) {
        std::vector<EnemyRobot*> enemies = level->getEnemies();
        const std::vector<float> falloff{1.0f, 0.7f, 0.5f};
        EnemyRobot* current = hitEnemies.front();
        for (int bounce = 1; bounce <= 3; ++bounce) {
            EnemyRobot* next = nullptr;
            int bestDist = 9999;
            for (EnemyRobot* e : enemies) {
                if (!e || e->isDefeated() || e == current) continue;
                if (std::find(hitEnemies.begin(), hitEnemies.end(), e) != hitEnemies.end()) continue;
                const int dist = std::abs(e->getPos().x - current->getPos().x) +
                                 std::abs(e->getPos().y - current->getPos().y);
                if (dist <= 5 && dist < bestDist) {
                    bestDist = dist;
                    next = e;
                }
            }
            if (!next) break;
            const int bdmg = std::max(1, static_cast<int>(std::round(static_cast<float>(damage) * falloff[std::min<int>(bounce, static_cast<int>(falloff.size() - 1))])));
            next->takeDamage(bdmg);
            m_model->pushCombatLogEntry("Chain bounce E" + std::to_string(next->getId()) + " -" + std::to_string(bdmg));
            hitEnemies.push_back(next);
            current = next;
        }
    }

    if (isTurretDeploy) {
        const std::vector<util::Vector2i> ring{
            {player->getPos().x + 1, player->getPos().y}, {player->getPos().x - 1, player->getPos().y},
            {player->getPos().x, player->getPos().y + 1}, {player->getPos().x, player->getPos().y - 1}
        };
        for (const auto& p : ring) {
            if (!level->isInBounds(p)) continue;
            Robot* r = level->getRobotAt(p);
            auto* e = dynamic_cast<EnemyRobot*>(r);
            if (!e) continue;
            e->takeDamage(3);
            m_model->pushCombatLogEntry("Turret pulse E" + std::to_string(e->getId()) + " -3");
        }
    }
}

// ── Enemy movement ────────────────────────────────────────────────────────────
void RuleManager::resolveEnemyMove(const Action& action,
                                    EnemyRobot* enemy, Level* level) {
    if (!enemy || !level) return;
    const util::Vector2i startPos = enemy->getPos();
    util::Vector2i target{action.targetX, action.targetY};
    if (!level->isInBounds(target)) return;
    if (level->getRobotAt(target) == nullptr) {
        level->moveRobot(enemy, target);
        enemy->setRecentActionLabel("MOVE", 1);
        m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " move -> (" + std::to_string(target.x) + "," + std::to_string(target.y) + ")");
        if (enemy->isFastMover()) {
            util::Vector2i delta{target.x - startPos.x, target.y - startPos.y};
            util::Vector2i step{target.x + delta.x, target.y + delta.y};
            if (level->isInBounds(step) && level->getTileRef(step.x, step.y).isWalkable()) {
                level->moveRobot(enemy, step);
                m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " fast -> (" + std::to_string(step.x) + "," + std::to_string(step.y) + ")");
            }
        }
    } else {
        enemy->setRecentActionLabel("BLOCKED", 1);
    }
}

// ── Enemy attack ──────────────────────────────────────────────────────────────
void RuleManager::resolveEnemyAttack(const Action& action, EnemyRobot* enemy,
                                      PlayerRobot* player, Level* level) {
    if (!enemy || !player || !level) return;
    if (action.moveType != util::MoveType::ATTACK) return;

    int damage = (action.damage > 0) ? action.damage : enemy->getAttackPower();
    if (action.moveType == util::MoveType::ATTACK && !enemy->consumeAttackDelayReady()) {
        enemy->setRecentActionLabel("WINDUP", 1);
        m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " windup");
        return;
    }
    damage = static_cast<int>(std::ceil(static_cast<float>(damage) * enemy->consumeDamageMultiplierOnHit()));
    if (auto b = m_eventBus.lock()) {
        b->publish(GameEvent{util::EventType::ATTACK_PERFORMED, enemy, player, 0});
    }
    Robot* target = level->getRobotAt({action.targetX, action.targetY});
    if (target == player) {
        player->takeDamage(damage);
        if (enemy->getDisableOnHitTurns() > 0) {
            player->setDisabledTurns(enemy->getDisableOnHitTurns());
            m_model->pushCombatLogEntry("P disabled " + std::to_string(enemy->getDisableOnHitTurns()) + "t");
        }
        const int reflected = player->applyReactiveArmor(damage);
        if (reflected > 0) {
            enemy->takeDamage(reflected);
            m_model->pushCombatLogEntry("P reflect -" + std::to_string(reflected) + " to E" + std::to_string(enemy->getId()));
            if (enemy->isDefeated() && enemy->getExplodeOnDeathDamage() > 0) {
                const util::Vector2i epos = enemy->getPos();
                const std::vector<util::Vector2i> splash{
                    {epos.x + 1, epos.y}, {epos.x - 1, epos.y},
                    {epos.x, epos.y + 1}, {epos.x, epos.y - 1}
                };
                for (const auto& p : splash) {
                    if (!level->isInBounds(p)) continue;
                    Robot* splashTarget = level->getRobotAt(p);
                    if (splashTarget) {
                        splashTarget->takeDamage(enemy->getExplodeOnDeathDamage());
                    }
                }
                m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " exploded");
            }
        }
        enemy->setRecentActionLabel("HIT", 1);
        m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " hit P -" + std::to_string(damage));
        if (auto b = m_eventBus.lock())
            b->publish(GameEvent{util::EventType::DAMAGE_DEALT, enemy, player, damage});
        if (player->isDefeated()) {
            m_model->pushCombatLogEntry("Player destroyed");
            if (auto b = m_eventBus.lock())
                b->publish(GameEvent{util::EventType::ROBOT_DEFEATED, enemy, player, 0});
        }

        // Heat gain on receiving damage
        const int heatGain = 1;
        player->addHeat(heatGain);
        m_model->pushCombatLogEntry("P gained " + std::to_string(heatGain) + " heat from being hit");

        // Component damage when player is hit below half HP
        const int halfHp = player->getMaxHp() / 2;
        if (player->getHp() <= halfHp) {
            // Find all equipped components with durability
            std::vector<Component*> durableComponents;
            for (int slot = 0; slot < RobotBody::kSlotCount; ++slot) {
                Component* comp = player->getBody()->getComponent(slot);
                if (comp && comp->hasDurability() && !comp->isDisabled()) {
                    durableComponents.push_back(comp);
                }
            }
            if (!durableComponents.empty()) {
                // Randomly select one to damage
                std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<size_t> dist(0, durableComponents.size() - 1);
                Component* selected = durableComponents[dist(rng)];
                selected->damage(1);
                player->setRecentActionLabel("COMPONENT DAMAGED!", 1);
                m_model->pushCombatLogEntry("P component damaged: " + selected->getName());
            }
        }
    } else {
        enemy->setRecentActionLabel("MISS", 1);
        m_model->pushCombatLogEntry("E" + std::to_string(enemy->getId()) + " attack miss");
    }

}

void RuleManager::activateHazards() {
    if (auto* level = m_model->getLevel()) level->updateHazards();
}

bool RuleManager::checkWinCondition() const {
    auto* level = m_model->getLevel();
    return level && level->getEnemies().empty();
}

bool RuleManager::checkLossCondition() const {
    auto* player = m_model->getPlayer();
    return player && player->isDefeated();
}
