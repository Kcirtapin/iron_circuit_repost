#include "model/Weapon.h"
#include "model/Level.h"
#include "util/Types.h"

static bool isWallAt(const Level* level, util::Vector2i p) {
    if (!level) return false;
    if (!level->isInBounds(p)) return true;
    return level->getTile(p.x, p.y).type == util::TileType::WALL;
}

std::vector<util::Vector2i> Weapon::getAttackTiles(util::Vector2i pos, util::Direction facing, const Level* level) const {
    std::vector<util::Vector2i> tiles;
    util::Vector2i fwd = util::directionOffset(facing);

    switch (m_pattern) {
        case AttackPattern::FRONT_TILE: {
            tiles.push_back({pos.x + fwd.x, pos.y + fwd.y});
            break;
        }
        case AttackPattern::LINE_FIXED: {
            for (int i = 1; i <= m_range; ++i) {
                tiles.push_back({pos.x + fwd.x * i, pos.y + fwd.y * i});
            }
            break;
        }
        case AttackPattern::RAY_UNTIL_WALL: {
            for (int i = 1; i <= m_range; ++i) {
                util::Vector2i p{pos.x + fwd.x * i, pos.y + fwd.y * i};
                if (!level) {
                    tiles.push_back(p);
                    continue;
                }
                if (!level->isInBounds(p)) break;
                if (isWallAt(level, p)) break;
                tiles.push_back(p);
            }
            break;
        }
        case AttackPattern::ARC_90_1: {
            // 3-tile fan at distance 1.
            util::Direction leftDir = util::resolveRelative(facing, util::RelativeDir::LEFT);
            util::Direction rightDir = util::resolveRelative(facing, util::RelativeDir::RIGHT);
            util::Vector2i left = util::directionOffset(leftDir);
            util::Vector2i right = util::directionOffset(rightDir);
            tiles.push_back({pos.x + fwd.x, pos.y + fwd.y});
            tiles.push_back({pos.x + fwd.x + left.x, pos.y + fwd.y + left.y});
            tiles.push_back({pos.x + fwd.x + right.x, pos.y + fwd.y + right.y});
            break;
        }
        default:
            break;
    }

    return tiles;
}

