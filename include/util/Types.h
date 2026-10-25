#ifndef IRON_CIRCUIT_UTIL_TYPES_H
#define IRON_CIRCUIT_UTIL_TYPES_H

#include <vector>
#include <string>

namespace util
{
    struct Vector2i
    {
        int x = 0, y = 0;
    };

    enum class Direction
    {
        UP,
        DOWN,
        LEFT,
        RIGHT,
        NONE
    };
    enum class RelativeDir
    {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT
    };
    enum class MoveType
    {
        NONE,
        ATTACK,
        MOVE,
        SPECIAL
    };
    enum class DrawLayer
    {
        BACKGROUND,
        TILES,
        HAZARDS,
        ROBOTS,
        EFFECTS,
        UI_WORLD,
        UI_OVERLAY
    };
    enum class TileType
    {
        FLOOR,
        WALL,
        FIRE,
        ACID,
        MODULE,
        EMPTY
    };
    enum class GameState
    {
        NONE,
        IN_GAME,
        VICTORY,
        DEFEAT
    };
    enum class WeaponType
    {
        MELEE,
        RANGED
    };
    enum class EventType
    {
        TURN_START,
        TURN_END,
        ATTACK_PERFORMED,
        DAMAGE_DEALT,
        ROBOT_DEFEATED
    };

    // Resolve a relative direction to a world direction given a facing direction
    inline Direction resolveRelative(Direction facing, RelativeDir rel)
    {
        // Map facing to index: UP=0, RIGHT=1, DOWN=2, LEFT=3
        int facingIdx = 0;
        switch (facing)
        {
        case Direction::UP:
            facingIdx = 0;
            break;
        case Direction::RIGHT:
            facingIdx = 1;
            break;
        case Direction::DOWN:
            facingIdx = 2;
            break;
        case Direction::LEFT:
            facingIdx = 3;
            break;
        default:
            facingIdx = 0;
            break;
        }
        int relOffset = 0;
        switch (rel)
        {
        case RelativeDir::FORWARD:
            relOffset = 0;
            break;
        case RelativeDir::RIGHT:
            relOffset = 1;
            break;
        case RelativeDir::BACKWARD:
            relOffset = 2;
            break;
        case RelativeDir::LEFT:
            relOffset = 3;
            break;
        }
        int resultIdx = (facingIdx + relOffset) % 4;
        switch (resultIdx)
        {
        case 0:
            return Direction::UP;
        case 1:
            return Direction::RIGHT;
        case 2:
            return Direction::DOWN;
        case 3:
            return Direction::LEFT;
        default:
            return Direction::UP;
        }
    }

    // Get the dx,dy offset for a world direction
    inline Vector2i directionOffset(Direction d)
    {
        switch (d)
        {
        case Direction::UP:
            return {0, -1};
        case Direction::DOWN:
            return {0, 1};
        case Direction::LEFT:
            return {-1, 0};
        case Direction::RIGHT:
            return {1, 0};
        default:
            return {0, 0};
        }
    }

    struct CardConfig
    {
        std::string name;
        std::string description;
        std::string moveType;
        int range = 1;
        int damage = 0;
    };
    struct PlayerConfig
    {
        bool valid = false;
        int startX = 1, startY = 1;
        int maxHp = 12;
        int attackPower = 2;
        int defense = 0;
        int maxHeat = 10;
        std::vector<CardConfig> cards;
    };
    struct EnemyConfig
    {
        bool valid = false;
        int x = 0, y = 0;
        int maxHp = 6;
        int attackPower = 1;
        int range = 1;
        std::string behavior;
    };
}

#endif // IRON_CIRCUIT_UTIL_TYPES_H
