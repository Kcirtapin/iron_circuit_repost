#include "model/Level.h"
#include "model/EnemyRobot.h"
#include "model/Robot.h"
#include "render/DrawQueue.h"
#include "util/GameConfig.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <string>

namespace {
constexpr int kFloorAtlasCols = 4;
constexpr int kFloorAtlasRows = 2;
constexpr int kFloorAtlasTiles = kFloorAtlasCols * kFloorAtlasRows;
constexpr int kFloorAtlasSizePx = 2048;
constexpr int kWallAtlasDivisions = 3;
constexpr int kWallAtlasSizePx = 2048;

util::TileType tileTypeFromChar(char ch)
{
    switch (ch)
    {
    case '#':
        return util::TileType::WALL;
    case 'F':
        return util::TileType::FIRE;
    case 'A':
        return util::TileType::ACID;
    case 'M':
        return util::TileType::MODULE;
    case 'V':
        return util::TileType::EMPTY;
    case '.':
    default:
        return util::TileType::FLOOR;
    }
}

bool tileCountsAsWall(const Level &level, int x, int y)
{
    return level.isInBounds(x, y) && level.getTile(x, y).type == util::TileType::WALL;
}

Rect floorAtlasSrcForTile(util::Vector2i pos)
{
    const unsigned hash = static_cast<unsigned>(pos.x * 73856093u) ^
                          static_cast<unsigned>(pos.y * 19349663u);
    const int variant = static_cast<int>(hash % static_cast<unsigned>(kFloorAtlasTiles));
    const int col = variant % kFloorAtlasCols;
    const int row = variant / kFloorAtlasCols;
    const int x0 = (col * kFloorAtlasSizePx) / kFloorAtlasCols;
    const int x1 = ((col + 1) * kFloorAtlasSizePx) / kFloorAtlasCols;
    const int y0 = (row * kFloorAtlasSizePx) / kFloorAtlasRows;
    const int y1 = ((row + 1) * kFloorAtlasSizePx) / kFloorAtlasRows;
    return Rect{x0, y0, x1 - x0, y1 - y0};
}

Rect wallAtlasSrcForTile(const Level &level, util::Vector2i pos)
{
    const bool openTop = !tileCountsAsWall(level, pos.x, pos.y - 1);
    const bool openBottom = !tileCountsAsWall(level, pos.x, pos.y + 1);
    const bool openLeft = !tileCountsAsWall(level, pos.x - 1, pos.y);
    const bool openRight = !tileCountsAsWall(level, pos.x + 1, pos.y);

    const int row = (openTop == openBottom) ? 1 : (openTop ? 0 : 2);
    const int col = (openLeft == openRight) ? 1 : (openLeft ? 0 : 2);
    const int x0 = (col * kWallAtlasSizePx) / kWallAtlasDivisions;
    const int x1 = ((col + 1) * kWallAtlasSizePx) / kWallAtlasDivisions;
    const int y0 = (row * kWallAtlasSizePx) / kWallAtlasDivisions;
    const int y1 = ((row + 1) * kWallAtlasSizePx) / kWallAtlasDivisions;
    return Rect{x0, y0, x1 - x0, y1 - y0};
}
} // namespace

bool Tile::isWalkable() const
{
    return type != util::TileType::WALL && type != util::TileType::EMPTY && occupant == nullptr;
}

Level::Level(int width, int height) : m_width(width), m_height(height)
{
    assert(width > 0 && height > 0);
    m_grid.resize(static_cast<size_t>(width * height));
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            auto &t = m_grid[static_cast<size_t>(y * width + x)];
            t.pos = {x, y};
            t.type = util::TileType::FLOOR;
        }
    }
}

Level::Level(const std::vector<std::string> &layout)
{
    assert(!layout.empty());
    m_height = static_cast<int>(layout.size());
    m_width = static_cast<int>(layout.front().size());
    m_grid.resize(static_cast<size_t>(m_width * m_height));

    for (int y = 0; y < m_height; ++y)
    {
        for (int x = 0; x < m_width; ++x)
        {
            auto &tile = m_grid[static_cast<size_t>(y * m_width + x)];
            tile.pos = {x, y};
            const char ch = (x < static_cast<int>(layout[static_cast<size_t>(y)].size())) ? layout[static_cast<size_t>(y)][static_cast<size_t>(x)] : '#';
            tile.type = tileTypeFromChar(ch);
            tile.occupant = nullptr;
        }
    }
}

bool Level::isInBounds(int x, int y) const
{
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

bool Level::isInBounds(util::Vector2i pos) const
{
    return isInBounds(pos.x, pos.y);
}

Tile Level::getTile(int x, int y) const
{
    if (!isInBounds(x, y))
        return Tile{};
    return m_grid[static_cast<size_t>(y * m_width + x)];
}

Tile &Level::getTileRef(int x, int y)
{
    assert(isInBounds(x, y));
    return m_grid[static_cast<size_t>(y * m_width + x)];
}

void Level::placeRobot(Robot *robot, util::Vector2i pos)
{
    if (!robot || !isInBounds(pos))
        return;
    Tile &t = getTileRef(pos.x, pos.y);
    if (!t.isWalkable())
        return;
    t.occupant = robot;
    robot->setPos(pos);
}

void Level::removeRobot(Robot *robot)
{
    if (!robot)
        return;
    for (auto &t : m_grid)
    {
        if (t.occupant == robot)
        {
            t.occupant = nullptr;
            return;
        }
    }
}

void Level::moveRobot(Robot *robot, util::Vector2i newPos)
{
    if (!robot || !isInBounds(newPos))
        return;
    Tile &target = getTileRef(newPos.x, newPos.y);
    if (!target.isWalkable())
        return;
    removeRobot(robot);
    target.occupant = robot;
    robot->setPos(newPos);
}

void Level::addEnemy(std::unique_ptr<EnemyRobot> enemy)
{
    if (enemy)
        m_enemies.push_back(std::move(enemy));
}

std::vector<EnemyRobot *> Level::getEnemies()
{
    std::vector<EnemyRobot *> out;
    for (auto &e : m_enemies)
        out.push_back(e.get());
    return out;
}

void Level::removeDefeatedEnemies()
{
    for (auto it = m_enemies.begin(); it != m_enemies.end();)
    {
        if ((*it)->isDefeated())
        {
            removeRobot((*it).get());
            it = m_enemies.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Level::updateHazards()
{
    // TODO: apply hazard tile damage to occupants
}

Robot *Level::getRobotAt(util::Vector2i pos) const
{
    if (!isInBounds(pos))
        return nullptr;
    return getTile(pos.x, pos.y).occupant;
}

void Level::submitDrawRequests(DrawQueue *queue, bool includeEnemies)
{
    if (!queue)
        return;

    const int cell = util::layout::CellSize;
    const int borderSize = util::layout::TileBorder;

    for (const auto &t : m_grid)
    {
        DrawRequest border;
        border.layer = util::DrawLayer::BACKGROUND;
        border.usePlaceholderShape = true;
        border.placeholderColor = 0xFF111116;
        border.dst.x = t.pos.x * cell;
        border.dst.y = t.pos.y * cell;
        border.dst.w = border.dst.h = cell;
        border.zOrder = 0;
        queue->push(border);

        DrawRequest req;
        unsigned tileColor;
        switch (t.type)
        {
        case util::TileType::WALL:
            tileColor = 0xFF6A707Cu;
            break;
        case util::TileType::FIRE:
            tileColor = 0xFF882200u;
            break;
        case util::TileType::ACID:
            tileColor = 0xFF226622u;
            break;
        case util::TileType::MODULE:
            tileColor = 0xFF3F586Eu;
            break;
        case util::TileType::EMPTY:
            tileColor = 0xFF080A10u;
            break;
        case util::TileType::FLOOR:
        default:
            tileColor = 0xFF3A3A44u;
            break;
        }
        req.layer = util::DrawLayer::TILES;
        req.usePlaceholderShape = true;
        req.placeholderColor = tileColor;
        if (t.type == util::TileType::WALL)
        {
            req.texturePath = "resource/image/WALL_TEXTURE.png";
            req.src = wallAtlasSrcForTile(*this, t.pos);
        }
        else if (t.type == util::TileType::FLOOR)
        {
            req.texturePath = "resource/image/FLOOR_TEXTURES.png";
            req.src = floorAtlasSrcForTile(t.pos);
        }
        req.dst.x = t.pos.x * cell + borderSize;
        req.dst.y = t.pos.y * cell + borderSize;
        req.dst.w = req.dst.h = cell - borderSize * 2;
        req.zOrder = t.pos.y * m_width + t.pos.x;
        queue->push(req);
    }

    if (includeEnemies) {
        for (auto &e : m_enemies)
            e->submitDrawRequests(queue);
    }
}
