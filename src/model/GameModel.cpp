#include "model/GameModel.h"
#include "controller/AIBehavior.h"
#include "events/EventBus.h"
#include "events/GameEvent.h"
#include "model/Component.h"
#include "model/EnemyRobot.h"
#include "model/Level.h"
#include "model/PlayerRobot.h"
#include "model/ProgramCard.h"
#include "model/RuleManager.h"
#include "model/Weapon.h"
#include "util/GameConfig.h"
#include "util/Types.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <random>
#include <queue>
#include <string>
#include <vector>
#include <fstream>

namespace
{
    struct EnemySpawnSpec
    {
        util::Vector2i pos;
        char type = 'M'; // LevelMaker robot symbols: M(melee), R(ranged), B(boss)
    };

    struct GeneratedLevelData
    {
        std::vector<std::string> layout;
        util::Vector2i playerStart{1, 1};
        std::vector<EnemySpawnSpec> enemySpawns;
    };

    util::MoveType moveTypeFromString(const std::string &s)
    {
        if (s == "attack") return util::MoveType::ATTACK;
        if (s == "move") return util::MoveType::MOVE;
        if (s == "special") return util::MoveType::SPECIAL;
        return util::MoveType::NONE;
    }

    ProgramCard makeMoveCard(std::string name, std::string description, std::vector<util::RelativeDir> path)
    {
        ProgramCard card;
        card.name = std::move(name);
        card.description = std::move(description);
        card.moveType = util::MoveType::MOVE;
        card.range = static_cast<int>(path.size());
        card.path = std::move(path);
        return card;
    }

    std::unique_ptr<Component> makeMoveModule(std::string name, std::string description, ProgramCard card)
    {
        return std::make_unique<ModuleComponent>(std::move(name), std::move(description), std::move(card));
    }

    char levelCharFromSymbol(char symbol)
    {
        switch (symbol)
        {
        case '+':
            return '#';
        case 'V':
        case ' ':
            return 'V';
        case '.':
        case 'P':
        case 'M':
        case 'R':
        case 'B':
        default:
            return '.';
        }
    }

    GeneratedLevelData buildGeneratedLevel(int levelIndex)
    {
        using RoomTemplate = std::vector<std::string>;

        // Room modules authored with LevelMaker symbols:
        // . floor, + wall, V/space void, and P/M/R/B robot markers.
        constexpr int kRoomCols = 4;
        constexpr int kRoomRows = 3;
        constexpr int kInnerW = 9;
        constexpr int kInnerH = 7;

        const std::vector<RoomTemplate> safeModules = {
            {
                ".........",
                "..+......",
                "....+....",
                "...P.....",
                "....+....",
                "......+..",
                ".........",
            },
            {
                "....+....",
                ".........",
                ".+.....+.",
                "..P......",
                ".+.....+.",
                ".........",
                "....+....",
            },
            {
                ".........",
                "..+...+..",
                ".........",
                "....P....",
                ".........",
                "..+...+..",
                ".........",
            },
        };
        const std::vector<RoomTemplate> skirmishModules = {
            {
                "...+.....",
                "..M...R..",
                ".........",
                ".+.....+.",
                "....M....",
                ".........",
                ".....+...",
            },
            {
                ".M.......",
                "...+.....",
                "....R....",
                ".........",
                "...+.....",
                "......M..",
                ".........",
            },
            {
                "....R....",
                "..+...+..",
                ".M.....M.",
                ".........",
                ".+.....+.",
                "....R....",
                ".........",
            },
        };
        const std::vector<RoomTemplate> clusterModules = {
            {
                "..MRM....",
                "..MMMM...",
                ".........",
                "..R+M....",
                "..MMM....",
                ".........",
                "...M+....",
            },
            {
                ".........",
                ".MRM.M...",
                "..MMMM...",
                "...RR....",
                "..+M.....",
                "..MMM....",
                ".........",
            },
            {
                "..RMM....",
                "..MRRM...",
                "..MMMM...",
                ".........",
                "...M+....",
                ".........",
                "....MR...",
            },
        };
        const std::vector<RoomTemplate> crossfireModules = {
            {
                "R.......R",
                "..+...+..",
                "...R.R...",
                "..MM.MM..",
                "...M.M...",
                "..+...+..",
                "R.......R",
            },
            {
                "..R.M.R..",
                "...R.R...",
                ".++...++.",
                "..MM.MM..",
                ".++...++.",
                "...R.R...",
                "..R.M.R..",
            },
            {
                "...RR....",
                ".+.....+.",
                "...M.M...",
                "R..MRM..R",
                "...M.M...",
                ".+.....+.",
                "...RR....",
            },
        };
        const std::vector<RoomTemplate> fortressModules = {
            {
                "..R.B.R..",
                ".++...++.",
                ".MM...MM.",
                "..M+++M..",
                ".MM...MM.",
                ".++...++.",
                "..R.M.R..",
            },
            {
                "..B.R....",
                ".+++..R..",
                ".MM.M....",
                "..M++M...",
                "....M.MR.",
                "..R..+++.",
                "....R.B..",
            },
        };

        const int width = kRoomCols * (kInnerW + 1) + 1;
        const int height = kRoomRows * (kInnerH + 1) + 1;
        GeneratedLevelData data;
        data.layout.assign(static_cast<size_t>(height), std::string(static_cast<size_t>(width), '#'));
        data.playerStart = {2, 2};

        std::mt19937 rng(static_cast<unsigned>(levelIndex * 7919 + 17));
        auto pickRoom = [&](const std::vector<RoomTemplate> &pool) -> const RoomTemplate & {
            std::uniform_int_distribution<int> roomDist(0, static_cast<int>(pool.size()) - 1);
            return pool[static_cast<size_t>(roomDist(rng))];
        };

        for (int roomY = 0; roomY < kRoomRows; ++roomY)
        {
            for (int roomX = 0; roomX < kRoomCols; ++roomX)
            {
                const RoomTemplate *room = nullptr;
                if (roomX == 0 && roomY == 0)
                {
                    room = &pickRoom(safeModules);
                }
                else
                {
                    const int roomBudget = levelIndex + roomX + roomY;
                    const bool clusterRoom =
                        ((roomX + roomY + levelIndex) % util::generation::ClusterRoomModulo) == 0;
                    const bool edgeRoom = roomX == (kRoomCols - 1) || roomY == (kRoomRows - 1);
                    if (levelIndex >= 4 && roomX == (kRoomCols - 1) && roomY == (kRoomRows - 1))
                        room = &pickRoom(fortressModules);
                    else if (clusterRoom && roomBudget >= 2)
                        room = &pickRoom(clusterModules);
                    else if (edgeRoom && roomBudget >= 3)
                        room = &pickRoom(crossfireModules);
                    else if (roomBudget >= 4)
                        room = &pickRoom(fortressModules);
                    else
                        room = &pickRoom(skirmishModules);
                }

                const int originX = roomX * (kInnerW + 1) + 1;
                const int originY = roomY * (kInnerH + 1) + 1;
                for (int iy = 0; iy < kInnerH; ++iy)
                {
                    for (int ix = 0; ix < kInnerW; ++ix)
                    {
                        const char ch = (*room)[static_cast<size_t>(iy)][static_cast<size_t>(ix)];
                        const int gx = originX + ix;
                        const int gy = originY + iy;
                        data.layout[static_cast<size_t>(gy)][static_cast<size_t>(gx)] = levelCharFromSymbol(ch);
                        if (ch == 'P')
                        {
                            data.playerStart = {gx, gy};
                        }
                        else if (ch == 'M' || ch == 'R' || ch == 'B')
                        {
                            data.enemySpawns.push_back(EnemySpawnSpec{{gx, gy}, ch});
                        }
                    }
                }
            }
        }

        for (int roomY = 0; roomY < kRoomRows; ++roomY)
        {
            for (int roomX = 0; roomX < kRoomCols - 1; ++roomX)
            {
                const int wallX = (roomX + 1) * (kInnerW + 1);
                const int doorY = roomY * (kInnerH + 1) + 1 + kInnerH / 2 + ((roomX + roomY + levelIndex) % 3) - 1;
                data.layout[static_cast<size_t>(doorY)][static_cast<size_t>(wallX)] = '.';
                if (doorY + 1 < height - 1)
                    data.layout[static_cast<size_t>(doorY + 1)][static_cast<size_t>(wallX)] = '.';
            }
        }

        for (int roomY = 0; roomY < kRoomRows - 1; ++roomY)
        {
            for (int roomX = 0; roomX < kRoomCols; ++roomX)
            {
                const int wallY = (roomY + 1) * (kInnerH + 1);
                const int doorX = roomX * (kInnerW + 1) + 1 + kInnerW / 2 + ((roomX + roomY + levelIndex) % 3) - 1;
                data.layout[static_cast<size_t>(wallY)][static_cast<size_t>(doorX)] = '.';
                if (doorX + 1 < width - 1)
                    data.layout[static_cast<size_t>(wallY)][static_cast<size_t>(doorX + 1)] = '.';
            }
        }

        data.layout[static_cast<size_t>(data.playerStart.y)][static_cast<size_t>(data.playerStart.x)] = '.';
        data.enemySpawns.erase(std::remove_if(data.enemySpawns.begin(), data.enemySpawns.end(),
                                              [&](const EnemySpawnSpec &spawn)
                                              { return spawn.pos.x == data.playerStart.x && spawn.pos.y == data.playerStart.y; }),
                              data.enemySpawns.end());

        // Difficulty ramp by round: larger levels can support thicker, more clustered packs.
        std::shuffle(data.enemySpawns.begin(), data.enemySpawns.end(), rng);
        const int desiredEnemyCount = std::min(
            static_cast<int>(data.enemySpawns.size()),
            std::max(util::generation::MinGeneratedEnemyCount,
                     util::generation::BaseGeneratedEnemyCount +
                         levelIndex * util::generation::GeneratedEnemyCountPerLevel));
        data.enemySpawns.resize(static_cast<size_t>(desiredEnemyCount));
        const bool hasRanged = std::any_of(data.enemySpawns.begin(), data.enemySpawns.end(),
                                           [](const EnemySpawnSpec& spawn) { return spawn.type == 'R'; });
        const bool hasMelee = std::any_of(data.enemySpawns.begin(), data.enemySpawns.end(),
                                          [](const EnemySpawnSpec& spawn) { return spawn.type == 'M'; });
        if (!hasRanged) {
            for (auto& spawn : data.enemySpawns) {
                if (spawn.type == 'M' || spawn.type == 'B') {
                    spawn.type = 'R';
                    break;
                }
            }
        }
        if (!hasMelee) {
            for (auto& spawn : data.enemySpawns) {
                if (spawn.type == 'R') {
                    spawn.type = 'M';
                    break;
                }
            }
        }
        return data;
    }

    bool hasLineOfSight(Level* level, util::Vector2i from, util::Vector2i to)
    {
        if (!level) return false;
        if (from.x != to.x && from.y != to.y) return false;
        if (from.x == to.x)
        {
            const int step = (to.y > from.y) ? 1 : -1;
            for (int y = from.y + step; y != to.y; y += step)
            {
                if (!level->isInBounds(from.x, y)) return false;
                const Tile& t = level->getTileRef(from.x, y);
                if (t.type == util::TileType::WALL || t.type == util::TileType::EMPTY)
                    return false;
            }
            return true;
        }
        const int step = (to.x > from.x) ? 1 : -1;
        for (int x = from.x + step; x != to.x; x += step)
        {
            if (!level->isInBounds(x, from.y)) return false;
            const Tile& t = level->getTileRef(x, from.y);
            if (t.type == util::TileType::WALL || t.type == util::TileType::EMPTY)
                return false;
        }
        return true;
    }

    int manhattan(util::Vector2i a, util::Vector2i b)
    {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    }

    struct EnemyArchetype {
        std::string id;
        std::string ai;
        int hp = 10;
        int damage = 2;
        int explodeOnDeathDamage = 0;
        int disableDuration = 0;
        int attackDelayTurns = 1;
        float firstHitMultiplier = 1.0f;
        float damageReductionPct = 0.0f;
        int regenPerTurn = 0;
        bool fastMover = false;
        int spawnIntervalTurns = 0;
        int maxMinions = 0;
    };

    std::vector<EnemyArchetype> loadEnemyArchetypesFromCatalog() {
        std::vector<EnemyArchetype> out;
        std::ifstream in("resource/config/content_catalog.json");
        if (!in.is_open()) return out;
        nlohmann::json j;
        try { in >> j; } catch (...) { return out; }
        if (!j.contains("enemies") || !j["enemies"].is_array()) return out;
        for (const auto& enemy : j["enemies"]) {
            EnemyArchetype a;
            a.id = enemy.value("id", "");
            a.ai = enemy.value("ai", "melee");
            a.hp = enemy.value("hp", 10);
            a.damage = enemy.value("damage", 2);
            const std::string speed = enemy.value("speed", "normal");
            a.fastMover = (speed == "fast");
            if (enemy.contains("mechanics") && enemy["mechanics"].is_array()) {
                for (const auto& mech : enemy["mechanics"]) {
                    const std::string m = mech.get<std::string>();
                    if (m == "explode_on_death")
                        a.explodeOnDeathDamage = enemy["params"].value("explosion_damage", 10);
                    else if (m == "disable_on_hit")
                        a.disableDuration = std::max(1, static_cast<int>(enemy["params"].value("disable_duration", 2.0) + 0.5));
                    else if (m == "windup")
                        a.attackDelayTurns = std::max(1, static_cast<int>(enemy.value("attack_delay", 2.0) + 0.5));
                    else if (m == "bonus_first_hit")
                        a.firstHitMultiplier = static_cast<float>(enemy["params"].value("first_hit_multiplier", 2.0));
                    else if (m == "damage_reduction")
                        a.damageReductionPct = static_cast<float>(enemy["params"].value("damage_reduction_pct", 0.5));
                    else if (m == "spawn_minions") {
                        a.spawnIntervalTurns = std::max(1, static_cast<int>(enemy["params"].value("spawn_interval", 5.0) + 0.5));
                        a.maxMinions = enemy["params"].value("max_minions", 3);
                    }
                }
            }
            out.push_back(std::move(a));
        }
        if (out.empty()) return out;

        // Apply global modifiers as baseline mutators to broaden prototype behavior.
        if (j.contains("modifiers") && j["modifiers"].is_array()) {
            for (const auto& mod : j["modifiers"]) {
                const std::string id = mod.value("id", "");
                if (id == "armored") {
                    const float dr = static_cast<float>(mod.value("damage_reduction_pct", 0.3));
                    for (auto& a : out) {
                        if (a.id == "tank" || a.id == "shield_drone") {
                            a.damageReductionPct = std::max(a.damageReductionPct, dr);
                        }
                    }
                } else if (id == "regenerating") {
                    const int regen = mod.value("hp_regen_per_sec", 2);
                    for (auto& a : out) {
                        if (a.id == "tank" || a.id == "summoner") {
                            a.regenPerTurn = std::max(a.regenPerTurn, regen);
                        }
                    }
                } else if (id == "explosive") {
                    const int dmg = mod.value("explosion_damage", 10);
                    for (auto& a : out) {
                        if (a.id == "kamikaze") {
                            a.explodeOnDeathDamage = std::max(a.explodeOnDeathDamage, dmg);
                        }
                    }
                }
            }
        }
        return out;
    }
} // namespace

GameModel::GameModel() : m_eventBus(EventBus::create())
{
    m_ruleManager = std::make_unique<RuleManager>(this, std::weak_ptr<EventBus>(m_eventBus));
}

GameModel::~GameModel() = default;

void GameModel::loadLevel(int levelIndex)
{
    const bool useGenerated = (levelIndex >= 1);
    GeneratedLevelData generated;
    if (useGenerated)
    {
        generated = buildGeneratedLevel(levelIndex);
        m_level = std::make_unique<Level>(generated.layout);
    }
    else
    {
        m_level = std::make_unique<Level>(8, 6);
    }

    util::PlayerConfig playerCfg = util::loadPlayerConfig();
    util::Vector2i startPos = useGenerated ? generated.playerStart : util::Vector2i{1, 3};
    int maxHp = 12;
    int attackPower = 2;
    int defense = 0;
    int maxHeat = 10;
    if (playerCfg.valid)
    {
        maxHp = playerCfg.maxHp;
        attackPower = playerCfg.attackPower;
        defense = playerCfg.defense;
        maxHeat = playerCfg.maxHeat;
        if (!useGenerated)
            startPos = {playerCfg.startX, playerCfg.startY};
    }

    m_player = std::make_unique<PlayerRobot>(0, startPos, std::weak_ptr<EventBus>(m_eventBus));
    m_currentLevel = std::max(1, levelIndex);
    m_player->updateInventoryCapacityForLevel(m_currentLevel);
    m_player->setMaxHp(maxHp);
    m_player->setHp(maxHp);
    m_player->setAttackPower(attackPower);
    m_player->setDefense(defense);
    m_player->setMaxHeat(maxHeat);
    m_player->setFacing(util::Direction::RIGHT);
    m_level->placeRobot(m_player.get(), startPos);
    m_player->configurePrototypeItems();

    m_player->getBody()->clearComponent(0);
    m_player->getBody()->clearComponent(1);
    m_player->getBody()->clearComponent(2);
    m_player->getBody()->clearComponent(3);

    if (playerCfg.valid && !playerCfg.cards.empty())
    {
        for (size_t i = 0; i < playerCfg.cards.size() && i < static_cast<size_t>(RobotBody::kSlotCount); ++i)
        {
            const util::CardConfig &c = playerCfg.cards[i];
            ProgramCard card;
            card.name = c.name;
            card.description = c.description;
            card.moveType = moveTypeFromString(c.moveType);
            card.range = c.range;
            if (card.moveType == util::MoveType::MOVE)
            {
                for (int r = 0; r < c.range; ++r)
                    card.path.push_back(util::RelativeDir::FORWARD);
            }

            if (i == 0)
            {
                int dmg = c.damage;
                if (dmg <= 0 && card.moveType == util::MoveType::ATTACK)
                    dmg = 1;
                auto weapon = std::make_unique<Weapon>(c.name, c.description,
                                                       util::WeaponType::MELEE, dmg, c.range, std::move(card));
                m_player->getBody()->equipComponent(std::move(weapon), 0);
            }
            else
            {
                auto module = std::make_unique<ModuleComponent>(c.name, c.description, std::move(card));
                m_player->getBody()->equipComponent(std::move(module), static_cast<int>(i));
            }
        }
    }
    else
    {
        ProgramCard swordCard;
        swordCard.name = "Sword";
        swordCard.description = "Slash the tile directly in front of you.";
        swordCard.moveType = util::MoveType::ATTACK;
        swordCard.range = 1;
        auto sword = std::make_unique<Weapon>("Sword", "Reliable melee slash.", util::WeaponType::MELEE, 3, 1, std::move(swordCard));
        m_player->getBody()->equipComponent(std::move(sword), 0);

        m_player->getBody()->equipComponent(
            makeMoveModule("Step", "Advance one tile.", makeMoveCard("Step", "Advance one tile.", {util::RelativeDir::FORWARD})), 1);
        m_player->getBody()->equipComponent(
            makeMoveModule("Dash", "Advance two tiles.", makeMoveCard("Dash", "Advance two tiles.", {util::RelativeDir::FORWARD, util::RelativeDir::FORWARD})), 2);
        m_player->getBody()->equipComponent(
            makeMoveModule("Sidestep", "Advance and shift right.", makeMoveCard("Sidestep", "Advance and shift right.", {util::RelativeDir::FORWARD, util::RelativeDir::RIGHT})), 3);
    }

    std::vector<EnemySpawnSpec> spawns;
    if (useGenerated)
        spawns = generated.enemySpawns;
    else
        spawns.push_back(EnemySpawnSpec{{4, 3}, 'M'});

    std::mt19937 rng(static_cast<unsigned>(levelIndex * 104729 + 31));
    const std::vector<EnemyArchetype> archetypes = loadEnemyArchetypesFromCatalog();
    std::vector<EnemyArchetype> meleeArchetypes;
    std::vector<EnemyArchetype> rangedArchetypes;
    for (const auto& a : archetypes) {
        if (a.ai == "ranged") rangedArchetypes.push_back(a);
        else meleeArchetypes.push_back(a);
    }
    std::shuffle(spawns.begin(), spawns.end(), rng);
    const int enemyCount = static_cast<int>(spawns.size());
    m_nextEnemyId = 1;
    for (int i = 0; i < enemyCount; ++i)
    {
        const EnemySpawnSpec spawnSpec = spawns[static_cast<size_t>(i)];
        auto enemy = std::make_unique<EnemyRobot>(m_nextEnemyId++, spawnSpec.pos, std::weak_ptr<EventBus>(m_eventBus));
        int hp = 4 + levelIndex * 2;
        int attackPower = 1 + levelIndex / 2;
        const char enemyType = spawnSpec.type;
        std::string spritePath = "resource/image/robot_sprite_enemy_melee.png";
        EnemyArchetype chosen;
        bool hasChosen = false;
        if (!archetypes.empty()) {
            if (enemyType == 'R' && !rangedArchetypes.empty()) {
                chosen = rangedArchetypes[static_cast<size_t>(i % rangedArchetypes.size())];
                hasChosen = true;
            } else if (!meleeArchetypes.empty()) {
                chosen = meleeArchetypes[static_cast<size_t>(i % meleeArchetypes.size())];
                hasChosen = true;
            }
        }

        if (enemyType == 'R')
        {
            spritePath = "resource/image/robot_sprite_enemy_ranged.png";
            if (!hasChosen) {
                hp = std::max(3, hp - 1);
                attackPower += 1;
                enemy->setArchetypeId("ranged");
            } else {
                hp = chosen.hp;
                attackPower = chosen.damage;
            }
            enemy->setBehavior(AIBehavior::ranged(enemy.get(), this, 4));
        }
        else if (enemyType == 'B')
        {
            if (!hasChosen) {
                hp += 4 + levelIndex;
                attackPower += 2;
                enemy->setArchetypeId("boss");
            } else {
                hp = chosen.hp + 20;
                attackPower = std::max(chosen.damage, 4);
            }
            enemy->setBehavior(AIBehavior::boss(enemy.get(), this));
        }
        else
        {
            if (hasChosen) {
                hp = chosen.hp;
                attackPower = chosen.damage;
            } else {
                enemy->setArchetypeId("melee");
            }
            enemy->setBehavior(AIBehavior::melee(enemy.get(), this));
        }
        hp += 3 + levelIndex * 2 + ((enemyType == 'B') ? 8 : 0);
        enemy->setMaxHp(hp);
        enemy->setHp(hp);
        enemy->setAttackPower(attackPower);
        enemy->setSpritePath(spritePath);
        if (hasChosen) {
            enemy->setArchetypeId(chosen.id);
            enemy->setExplodeOnDeathDamage(chosen.explodeOnDeathDamage);
            enemy->setDisableOnHitTurns(chosen.disableDuration);
            enemy->setAttackDelayTurns(chosen.attackDelayTurns);
            enemy->setFirstHitMultiplier(chosen.firstHitMultiplier);
            enemy->setDamageReductionPct(chosen.damageReductionPct);
            enemy->setHpRegenPerTurn(chosen.regenPerTurn);
            enemy->setFastMover(chosen.fastMover);
            enemy->setSummonerConfig(chosen.spawnIntervalTurns, chosen.maxMinions);
        }
        m_level->placeRobot(enemy.get(), spawnSpec.pos);
        m_level->addEnemy(std::move(enemy));
    }

    m_pendingPlayerAction.reset();
    m_pendingEnemyActions.clear();
    clearCombatLog();
    pushCombatLogEntry("Level " + std::to_string(m_currentLevel) + " started.");
    m_turnNumber = 0;
    m_state = util::GameState::IN_GAME;
}

bool GameModel::spawnEnemyMinion(util::Vector2i origin)
{
    if (!m_level) return false;
    const std::vector<util::Vector2i> candidates{
        {origin.x + 1, origin.y}, {origin.x - 1, origin.y},
        {origin.x, origin.y + 1}, {origin.x, origin.y - 1}
    };
    for (const auto& p : candidates) {
        if (!m_level->isInBounds(p) || !m_level->getTileRef(p.x, p.y).isWalkable()) continue;
        auto minion = std::make_unique<EnemyRobot>(m_nextEnemyId++, p, std::weak_ptr<EventBus>(m_eventBus));
        minion->setArchetypeId("minion");
        minion->setMaxHp(8);
        minion->setHp(8);
        minion->setAttackPower(2);
        minion->setBehavior(AIBehavior::melee(minion.get(), this));
        m_level->placeRobot(minion.get(), p);
        m_level->addEnemy(std::move(minion));
        return true;
    }
    return false;
}

void GameModel::submitPlayerAction(PlayerAction action)
{
    m_pendingPlayerAction = std::move(action);
}

void GameModel::submitEnemyAction(int enemyId, Action action)
{
    m_pendingEnemyActions.push_back({enemyId, std::move(action)});
}

void GameModel::collectEnemyActions()
{
    if (!m_pendingPlayerAction || !m_level)
        return;
    if (!m_pendingEnemyActions.empty())
        return;
    std::vector<EnemyRobot*> enemies = m_level->getEnemies();
    const util::Vector2i playerPos = m_player->getPos();
    const int visibleRadiusX = util::layout::WorldViewportW / (2 * util::layout::CellSize) + 1;
    const int visibleRadiusY = util::layout::WorldViewportH / (2 * util::layout::CellSize) + 1;

    for (EnemyRobot* e : enemies)
    {
        if (!e) continue;
        const util::Vector2i enemyPos = e->getPos();
        const bool touchedPlayer = (manhattan(enemyPos, playerPos) <= 1);
        const bool seesPlayer = hasLineOfSight(m_level.get(), enemyPos, playerPos);
        const bool onScreen = std::abs(enemyPos.x - playerPos.x) <= visibleRadiusX &&
                              std::abs(enemyPos.y - playerPos.y) <= visibleRadiusY;
        if (touchedPlayer || seesPlayer || onScreen)
            e->setAwareOfPlayer(true);
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (EnemyRobot* e : enemies)
        {
            if (!e || e->isAwareOfPlayer()) continue;
            for (EnemyRobot* ally : enemies)
            {
                if (!ally || !ally->isAwareOfPlayer()) continue;
                if (hasLineOfSight(m_level.get(), e->getPos(), ally->getPos()))
                {
                    e->setAwareOfPlayer(true);
                    changed = true;
                    break;
                }
            }
        }
    }

    const bool timeSlowActive = m_player->isTimeSlowActive();
    const bool skipThisEnemyTurn = timeSlowActive && ((m_turnNumber % 2) == 1);
    for (EnemyRobot *e : enemies)
    {
        if (skipThisEnemyTurn) {
            Action noop;
            e->setRecentActionLabel("SLOWED", 1);
            m_pendingEnemyActions.push_back({e->getId(), noop});
        } else {
            e->takeTurn();
            m_pendingEnemyActions.push_back({e->getId(), e->getTelegraphedAction()});
        }
    }
}

bool GameModel::allActionsReady() const
{
    if (!m_pendingPlayerAction.has_value())
        return false;
    Level *level = m_level.get();
    if (!level)
        return true;
    size_t enemyCount = level->getEnemies().size();
    return m_pendingEnemyActions.size() >= enemyCount;
}

void GameModel::executeTurn()
{
    if (!m_ruleManager || !m_level || !m_player)
        return;

    std::cout << "[GameModel] ==== Executing Turn " << m_turnNumber << " ====\n";
    m_player->onTurnStartItemTick();

    if (m_eventBus)
        m_eventBus->publish(GameEvent{util::EventType::TURN_START, nullptr, nullptr, 0});

    m_ruleManager->resolveSimultaneousActions(
        m_pendingPlayerAction,
        m_pendingEnemyActions);
    m_level->removeDefeatedEnemies();
    m_ruleManager->activateHazards();
    m_player->tickConditions();
    for (auto *e : m_level->getEnemies())
        e->tickConditions();

    if (m_ruleManager->checkWinCondition())
        m_state = util::GameState::VICTORY;
    else if (m_ruleManager->checkLossCondition())
        m_state = util::GameState::DEFEAT;

    if (m_eventBus)
        m_eventBus->publish(GameEvent{util::EventType::TURN_END, nullptr, nullptr, 0});

    m_pendingPlayerAction.reset();
    m_pendingEnemyActions.clear();
    m_turnNumber++;
}

void GameModel::applyUpgrade(std::unique_ptr<Component> component, int slot)
{
    if (m_player && component && slot >= 0 && slot < RobotBody::kSlotCount)
        m_player->getBody()->equipComponent(std::move(component), slot);
}

void GameModel::pushCombatLogEntry(std::string entry)
{
    if (entry.empty())
        return;
    m_combatLogEntries.push_back(std::move(entry));
    while (m_combatLogEntries.size() > kMaxCombatLogEntries)
        m_combatLogEntries.pop_front();
}

void GameModel::clearCombatLog()
{
    m_combatLogEntries.clear();
}
