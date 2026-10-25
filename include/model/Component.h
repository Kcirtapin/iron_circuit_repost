#ifndef IRON_CIRCUIT_MODEL_COMPONENT_H
#define IRON_CIRCUIT_MODEL_COMPONENT_H

#include "model/ProgramCard.h"
#include <algorithm>
#include <string>
#include <utility>

class Component {
public:
    virtual ~Component() = default;
    virtual const ProgramCard* getProgramCard() const { return nullptr; }
    virtual const std::string& getName() const {
        static const std::string kEmpty;
        return kEmpty;
    }
    virtual const std::string& getDescription() const {
        static const std::string kEmpty;
        return kEmpty;
    }

    int getCurrentHp() const { return m_currentHp; }
    int getMaxHp() const { return m_maxHp; }
    bool hasDurability() const { return m_maxHp > 0; }
    bool isDisabled() const {
        return m_maxHp > 0 && m_currentHp <= 0;
    }
    void setMaxHp(int hp) {
        m_maxHp = std::max(0, hp);
        m_currentHp = std::clamp(m_currentHp, 0, m_maxHp);
    }
    void setCurrentHp(int hp) {
        if (m_maxHp <= 0) {
            m_currentHp = 0;
            return;
        }
        m_currentHp = std::clamp(hp, 0, m_maxHp);
    }
    void resetRoundStatus() {
        if (m_maxHp > 0 && m_currentHp <= 0) {
            m_currentHp = m_maxHp;
        }
    }
    void damage(int amount) {
        if (m_maxHp <= 0) return;
        const int delta = std::max(1, amount);
        m_currentHp = std::max(0, m_currentHp - delta);
    }

private:
    int m_currentHp = 0;
    int m_maxHp = 0;
};

class ModuleComponent : public Component {
public:
    ModuleComponent(std::string name, std::string description, ProgramCard card,
                    int bonusMaxHp = 0, int bonusAttack = 0, int bonusDefense = 0, int bonusMaxHeat = 0)
        : m_name(std::move(name)),
          m_description(std::move(description)),
          m_card(std::move(card)),
          m_bonusMaxHp(bonusMaxHp),
          m_bonusAttack(bonusAttack),
          m_bonusDefense(bonusDefense),
          m_bonusMaxHeat(bonusMaxHeat) {}

    const ProgramCard* getProgramCard() const override { return &m_card; }
    void setProgramCard(ProgramCard card) { m_card = std::move(card); }
    const std::string& getName() const override { return m_name; }
    const std::string& getDescription() const override { return m_description; }
    int getBonusMaxHp() const { return m_bonusMaxHp; }
    int getBonusAttack() const { return m_bonusAttack; }
    int getBonusDefense() const { return m_bonusDefense; }
    int getBonusMaxHeat() const { return m_bonusMaxHeat; }

private:
    std::string m_name;
    std::string m_description;
    ProgramCard m_card;
    int m_bonusMaxHp = 0;
    int m_bonusAttack = 0;
    int m_bonusDefense = 0;
    int m_bonusMaxHeat = 0;
};

#endif // IRON_CIRCUIT_MODEL_COMPONENT_H