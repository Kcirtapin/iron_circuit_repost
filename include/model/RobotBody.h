#ifndef IRON_CIRCUIT_MODEL_ROBOTBODY_H
#define IRON_CIRCUIT_MODEL_ROBOTBODY_H

#include "model/ProgramCard.h"
#include "model/Component.h"
#include "model/Weapon.h"
#include <memory>
#include <vector>

class RobotBody {
public:
    static constexpr int kSlotCount = 20;
    static constexpr int kWeaponSlot = 0;

    Component* getComponent(int slot) {
        return (slot >= 0 && slot < kSlotCount) ? m_slots[slot].get() : nullptr;
    }

    const Component* getComponent(int slot) const {
        return (slot >= 0 && slot < kSlotCount) ? m_slots[slot].get() : nullptr;
    }

    bool equipComponent(std::unique_ptr<Component> c, int slot) {
        if (!c) return false;
        if (slot < 0 || slot >= kSlotCount) return false;
        m_slots[slot] = std::move(c);
        return true;
    }

    void clearComponent(int slot) {
        if (slot >= 0 && slot < kSlotCount) {
            m_slots[slot].reset();
        }
    }

    std::vector<ProgramCard> getAvailablePrograms() const {
        std::vector<ProgramCard> out;
        for (int i = 0; i < kSlotCount; i++) {
            if (m_slots[i]) {
                if (const ProgramCard* pc = m_slots[i]->getProgramCard()) {
                    out.push_back(*pc);
                }
            }
        }
        return out;
    }

private:
    std::unique_ptr<Component> m_slots[kSlotCount];
};

#endif // IRON_CIRCUIT_MODEL_ROBOTBODY_H