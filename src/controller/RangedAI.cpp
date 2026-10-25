#include "controller/RangedAI.h"
#include "controller/AIBehavior.h"
#include "model/EnemyRobot.h"
#include "model/GameModel.h"
#include "model/Level.h"
#include "model/PlayerRobot.h"
#include "util/Types.h"
#include <array>
#include <cstdlib>

namespace {
bool isWalkable(Level* level, util::Vector2i pos) {
    return level && level->isInBounds(pos) && level->getTileRef(pos.x, pos.y).isWalkable();
}

bool hasClearShot(Level* level, util::Vector2i from, util::Vector2i to) {
    if (!level) return false;
    if (from.x != to.x && from.y != to.y) return false;
    if (from.x == to.x) {
        const int step = (to.y > from.y) ? 1 : -1;
        for (int y = from.y + step; y != to.y; y += step) {
            if (!level->isInBounds(from.x, y)) return false;
            const Tile& tile = level->getTileRef(from.x, y);
            if (tile.type == util::TileType::WALL || tile.type == util::TileType::EMPTY || tile.occupant != nullptr)
                return false;
        }
        return true;
    }
    const int step = (to.x > from.x) ? 1 : -1;
    for (int x = from.x + step; x != to.x; x += step) {
        if (!level->isInBounds(x, from.y)) return false;
        const Tile& tile = level->getTileRef(x, from.y);
        if (tile.type == util::TileType::WALL || tile.type == util::TileType::EMPTY || tile.occupant != nullptr)
            return false;
    }
    return true;
}

int manhattan(util::Vector2i a, util::Vector2i b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}
}

RangedAI::RangedAI(EnemyRobot* robot, GameModel* model, int range)
    : AIBehavior(robot, model), m_range(range) {}

Action RangedAI::tick() {
    Action action;
    if (!m_robot || !m_model) return action;

    PlayerRobot* player = m_model->getPlayer();
    Level* level = m_model->getLevel();
    if (!player || !level) return action;
    if (!m_robot->isAwareOfPlayer()) return action;

    util::Vector2i myPos = m_robot->getPos();
    util::Vector2i pPos = player->getPos();
    int dx = pPos.x - myPos.x;
    int dy = pPos.y - myPos.y;
    int dist = std::abs(dx) + std::abs(dy);

    // If player is on same row or column and within range with clear line -> attack
    if (dist <= m_range && (dx == 0 || dy == 0) && dist > 0 && hasClearShot(level, myPos, pPos)) {
        action.moveType = util::MoveType::ATTACK;
        action.targetX = pPos.x;
        action.targetY = pPos.y;
        action.damage = m_robot->getAttackPower();
        return action;
    }

    action.moveType = util::MoveType::MOVE;
    // Pick the best neighboring tile that improves line setup while respecting walls.
    std::array<util::Vector2i, 4> candidates{{
        {myPos.x + 1, myPos.y}, {myPos.x - 1, myPos.y},
        {myPos.x, myPos.y + 1}, {myPos.x, myPos.y - 1}
    }};
    util::Vector2i best = myPos;
    int bestScore = -1000000;
    for (const auto& c : candidates) {
        if (!isWalkable(level, c)) continue;
        const int cDist = manhattan(c, pPos);
        int score = 0;
        const bool sameLine = (c.x == pPos.x || c.y == pPos.y);
        if (sameLine && hasClearShot(level, c, pPos)) score += 40;
        score -= std::abs(cDist - m_range) * 6;
        score -= cDist;
        // Retreat bias if too close.
        if (dist <= 1 && cDist > dist) score += 20;
        if (score > bestScore) {
            bestScore = score;
            best = c;
        }
    }
    if (best.x == myPos.x && best.y == myPos.y) {
        action.moveType = util::MoveType::NONE;
        return action;
    }
    action.targetX = best.x;
    action.targetY = best.y;
    return action;
}

// Factory
std::unique_ptr<AIBehavior> AIBehavior::ranged(EnemyRobot* owner, GameModel* model, int range) {
    return std::make_unique<RangedAI>(owner, model, range);
}
