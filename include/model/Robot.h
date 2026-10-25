#ifndef IRON_CIRCUIT_MODEL_ROBOT_H
#define IRON_CIRCUIT_MODEL_ROBOT_H

#include "util/Types.h"
#include "model/RobotBody.h"
#include <memory>

class Robot {
public:
    Robot(int id, util::Vector2i pos, std::weak_ptr<class EventBus> eb)
        : m_id(id), m_pos(pos), m_eventBus(std::move(eb)) {}
    virtual ~Robot() = default;

    // Identity & position
    int getId() const { return m_id; }
    util::Vector2i getPos() const { return m_pos; }
    void setPos(util::Vector2i p) { m_pos = p; }

    // Facing
    util::Direction getFacing() const { return m_facing; }
    void setFacing(util::Direction d) { m_facing = d; }

    // HP
    int getHp() const { return m_hp; }
    int getMaxHp() const { return m_maxHp; }
    void setMaxHp(int hp) { m_maxHp = hp; }
    void heal(int v) { m_hp = std::min(m_hp + v, m_maxHp); }
    void setHp(int v) { m_hp = v; }

    // Combat stats
    int getAttackPower() const { return m_attackPower; }
    void setAttackPower(int v) { m_attackPower = v; }
    int getDefense() const { return m_defense; }
    void setDefense(int d) { m_defense = d; }

    // Heat
    int getHeat() const { return m_heat; }
    int getMaxHeat() const { return m_maxHeat; }
    void setMaxHeat(int h) { m_maxHeat = h; }
    void addHeat(int h) { m_heat = std::min(m_heat + h, m_maxHeat); }
    void coolHeat(int h) { m_heat = std::max(m_heat - h, 0); }

    // Damage & defeat
    virtual void takeDamage(int d) {
        int actual = std::max(d - m_defense, 1);
        m_hp -= actual;
    }
    virtual bool isDefeated() const { return m_hp <= 0; }
    virtual bool isInvulnerable() const { return false; }

    // Body / components
    RobotBody* getBody() { return &m_body; }

    // Draw & conditions
    virtual void submitDrawRequests(class DrawQueue* q) { (void)q; }
    virtual void tickConditions() {}

protected:
    int m_id = 0;
    util::Vector2i m_pos;
    util::Direction m_facing = util::Direction::RIGHT;
    int m_hp = 1, m_maxHp = 1;
    int m_attackPower = 1;
    int m_defense = 0;
    int m_heat = 0, m_maxHeat = 10;
    RobotBody m_body;
    std::weak_ptr<class EventBus> m_eventBus;
};

#endif // IRON_CIRCUIT_MODEL_ROBOT_H
