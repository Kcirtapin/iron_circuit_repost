#include "controller/MeleeAI.h"
#include "controller/AIBehavior.h"
#include "model/EnemyRobot.h"
#include "model/GameModel.h"
#include "model/Level.h"
#include "model/PlayerRobot.h"
#include "util/Types.h"
#include <array>
#include <cstdlib>
#include <queue>
#include <unordered_map>

namespace {
bool isWalkable(Level* level, util::Vector2i pos) {
    return level && level->isInBounds(pos) && level->getTileRef(pos.x, pos.y).isWalkable();
}

util::Vector2i bfsNextStep(Level* level, util::Vector2i start, util::Vector2i goal) {
    if (!level || start.x == goal.x && start.y == goal.y) return start;
    const std::array<util::Vector2i, 4> dirs{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
    std::queue<util::Vector2i> q;
    std::unordered_map<int, util::Vector2i> parent;
    const int width = level->getWidth();
    auto key = [width](util::Vector2i p) { return p.y * width + p.x; };
    q.push(start);
    parent[key(start)] = start;

    while (!q.empty()) {
        const util::Vector2i cur = q.front();
        q.pop();
        if (cur.x == goal.x && cur.y == goal.y) break;
        for (const auto& d : dirs) {
            util::Vector2i next{cur.x + d.x, cur.y + d.y};
            if (!level->isInBounds(next)) continue;
            if (!(next.x == goal.x && next.y == goal.y) && !isWalkable(level, next)) continue;
            const int k = key(next);
            if (parent.find(k) != parent.end()) continue;
            parent[k] = cur;
            q.push(next);
        }
    }

    const int goalKey = key(goal);
    if (parent.find(goalKey) == parent.end()) {
        return start;
    }
    util::Vector2i cur = goal;
    util::Vector2i prev = goal;
    while (!(cur.x == start.x && cur.y == start.y)) {
        prev = cur;
        cur = parent[key(cur)];
    }
    return prev;
}
}

MeleeAI::MeleeAI(EnemyRobot* robot, GameModel* model)
    : AIBehavior(robot, model) {}

Action MeleeAI::tick() {
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

    // Adjacent → attack the player's tile
    if (dist == 1) {
        action.moveType = util::MoveType::ATTACK;
        action.targetX = pPos.x;
        action.targetY = pPos.y;
        action.damage = m_robot->getAttackPower();
        return action;
    }

    // Not adjacent -> path toward player using BFS.
    action.moveType = util::MoveType::MOVE;
    util::Vector2i target = bfsNextStep(level, myPos, pPos);
    if (target.x == myPos.x && target.y == myPos.y) {
        action.moveType = util::MoveType::NONE;
        return action;
    }

    action.targetX = target.x;
    action.targetY = target.y;
    return action;
}

// Factory
std::unique_ptr<AIBehavior> AIBehavior::melee(EnemyRobot* owner, GameModel* model) {
    return std::make_unique<MeleeAI>(owner, model);
}
