#ifndef IRON_CIRCUIT_MODEL_LEVEL_H
#define IRON_CIRCUIT_MODEL_LEVEL_H

#include "util/Types.h"
#include <memory>
#include <string>
#include <vector>

class Robot;
class EnemyRobot;
class DrawQueue;

struct Tile
{
    util::Vector2i pos;
    util::TileType type = util::TileType::FLOOR;
    Robot *occupant = nullptr;
    bool isWalkable() const;
};

class Level
{
public:
    Level(int width, int height);
    explicit Level(const std::vector<std::string> &layout);
    bool isInBounds(int x, int y) const;
    bool isInBounds(util::Vector2i pos) const;
    Tile getTile(int x, int y) const;
    Tile &getTileRef(int x, int y);
    void placeRobot(Robot *robot, util::Vector2i pos);
    void removeRobot(Robot *robot);
    void moveRobot(Robot *robot, util::Vector2i newPos);
    void addEnemy(std::unique_ptr<EnemyRobot> enemy);
    std::vector<EnemyRobot *> getEnemies();
    void removeDefeatedEnemies();
    void updateHazards();
    Robot *getRobotAt(util::Vector2i pos) const;
    void submitDrawRequests(DrawQueue *queue, bool includeEnemies = true);
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<Tile> m_grid;
    std::vector<std::unique_ptr<EnemyRobot>> m_enemies;
};

#endif // IRON_CIRCUIT_MODEL_LEVEL_H
