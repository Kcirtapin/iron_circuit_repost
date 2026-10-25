#ifndef IRON_CIRCUIT_MODEL_PLAYERROBOT_H
#define IRON_CIRCUIT_MODEL_PLAYERROBOT_H

#include "model/Robot.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "events/EventBus.h"

class PlayerRobot : public Robot {
public:
    PlayerRobot(int id, util::Vector2i pos, std::weak_ptr<EventBus> eventBus);

    // Inventory (2 spare component slots)
    static constexpr int kStartingInventorySlots = 2;
    static constexpr int kMaxInventorySlots = 256;
    static constexpr int kInventorySlotsPerLevel = 2;

    void updateInventoryCapacityForLevel(int level);
    int getInventoryCapacity() const { return m_inventoryCapacity; }

    bool storeInInventory(std::unique_ptr<Component> component);
    std::unique_ptr<Component> dropFromInventory(int slot);

    // Programs from equipped components
    std::vector<ProgramCard> getAvailablePrograms() const;
    void setProgramList(std::vector<ProgramCard> programs);
    void addProgramCard(const ProgramCard& card);
    const Weapon* getActiveWeapon() const;
    int getActiveWeaponSlot() const { return m_activeWeaponSlot; }
    bool setActiveWeaponSlot(int slot);
    void cycleActiveWeapon(int dir);
    void resetActiveWeaponSlot();
    std::vector<int> getWeaponSlots() const;
    void configurePrototypeItems();
    void setSelectedItemIds(std::vector<std::string> ids) { m_selectedItemIds = std::move(ids); }
    void onTurnStartItemTick();
    bool isTimeSlowActive() const { return m_timeSlowTurns > 0; }
    bool isOverclockActive() const { return m_overclockTurns > 0; }
    float getLifestealPct() const { return m_lifestealPct; }
    int applyReactiveArmor(int incomingDamage);
    bool tryBlinkMove(class Level* level, util::Direction dir);
    void setDisabledTurns(int turns) { m_disabledTurns = std::max(m_disabledTurns, turns); }
    bool isDisabled() const { return m_disabledTurns > 0; }

    void setRecentActionLabel(std::string text, int turnsToShow = 1);
    void clearRecentActionLabel();
    void decayRecentActionLabel();
    void takeDamage(int d) override;

    // Rendering
    void submitDrawRequests(class DrawQueue* queue) override;
    void submitDrawRequests(class DrawQueue* queue, int pixelOffsetX, int pixelOffsetY);

private:

private:
    int m_inventoryCapacity = kStartingInventorySlots;
    std::vector<std::unique_ptr<Component>> m_inventory;
    int m_activeWeaponSlot = RobotBody::kWeaponSlot;
    std::vector<ProgramCard> m_programList;
    float m_lifestealPct = 0.0f;
    std::string m_recentActionLabel;
    int m_recentActionTurns = 0;
    float m_reflectPct = 0.0f;
    int m_reflectCooldownTurns = 0;
    int m_reflectCooldownRemaining = 0;
    int m_timeSlowTurns = 0;
    int m_overclockTurns = 0;
    int m_overclockSelfDamagePerTurn = 0;
    int m_blinkCooldownTurns = 0;
    int m_blinkCooldownRemaining = 0;
    int m_blinkRange = 0;
    int m_disabledTurns = 0;
    std::vector<std::string> m_selectedItemIds;
};

#endif // IRON_CIRCUIT_MODEL_PLAYERROBOT_H
