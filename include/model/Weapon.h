#ifndef IRON_CIRCUIT_MODEL_WEAPON_H
#define IRON_CIRCUIT_MODEL_WEAPON_H

#include "model/Component.h"
#include "util/Types.h"
#include <memory>
#include <vector>

class Level;

class Weapon : public Component {
public:
    enum class AttackPattern {
        FRONT_TILE,        // melee: 1 tile ahead
        LINE_FIXED,        // line forward, fixed length = range
        RAY_UNTIL_WALL,    // line forward until wall or bounds (max = range)
        ARC_90_1           // 3-tile fan: forward + forward-left + forward-right (distance 1)
    };

    Weapon(std::string name, std::string desc, util::WeaponType wt, int dmg, int range, ProgramCard card,
           AttackPattern pattern = AttackPattern::FRONT_TILE)
        : m_name(std::move(name)), m_desc(std::move(desc)), m_type(wt),
          m_damage(dmg), m_range(range), m_card(std::move(card)), m_pattern(pattern) {}

    const std::string& getName() const { return m_name; }
    const std::string& getDescription() const { return m_desc; }
    int getDamage() const { return m_damage; }
    int getRange() const { return m_range; }
    util::WeaponType getWeaponType() const { return m_type; }
    AttackPattern getAttackPattern() const { return m_pattern; }
    const ProgramCard* getProgramCard() const override { return &m_card; }

    // Returns world-space tiles that this weapon attacks from the given position+facing.
    // Level is optional; patterns like RAY_UNTIL_WALL use it to stop at walls.
    std::vector<util::Vector2i> getAttackTiles(util::Vector2i pos, util::Direction facing, const Level* level) const;

private:
    std::string m_name;
    std::string m_desc;
    util::WeaponType m_type;
    int m_damage = 0;
    int m_range = 1;
    ProgramCard m_card;
    AttackPattern m_pattern = AttackPattern::FRONT_TILE;
};

#endif // IRON_CIRCUIT_MODEL_WEAPON_H
