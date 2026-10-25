#ifndef IRON_CIRCUIT_VIEW_HUMANVIEW_H
#define IRON_CIRCUIT_VIEW_HUMANVIEW_H

#include "model/ProgramCard.h"
#include "util/Types.h"
#include <optional>
#include <string>
#include <vector>
#include <SDL_events.h>

class DrawQueue;

class HumanView {
public:
    struct EquipmentEntry {
        int slot = -1;
        bool isWeapon = false;
        bool isActiveWeapon = false;
        std::string label;
        std::string description;
    };

    HumanView() = default;
    void handleSDLEvent(const SDL_Event& event);
    void updateSelectedCardFromIndex();
    void setAvailableCards(std::vector<ProgramCard> cards);
    bool hasConfirmed() const;
    std::optional<ProgramCard> getSelectedCard() const;
    std::optional<util::Direction> getPendingDirection() const;
    void setPlayerStats(int hp, int maxHp, int heat, int maxHeat);
    void setWeaponSummary(std::string weaponName, std::string weaponDescription);
    void setCombatLog(std::vector<std::string> combatLog);
    void setWorldHoverText(std::string text);
    int getLastMouseX() const { return m_lastMouseX; }
    int getLastMouseY() const { return m_lastMouseY; }
    void setEquipmentEntries(std::vector<EquipmentEntry> entries);
    std::optional<int> consumeRequestedWeaponSlot();
    int consumeWeaponCycleRequest();
    void clearSelection();
    void submitDrawRequests(DrawQueue* queue);
private:
    struct Rect { int x, y, w, h; };
    static constexpr int s_cardSlots = 5;
    static constexpr int s_equipmentVisibleRows = 5;

    Rect getCardRect(int index) const;
    Rect getConfirmRect() const;
    Rect getEquipmentPanelRect() const;
    static bool pointInRect(int px, int py, const Rect& r);
    void cycleDirectionClockwise();
    static const char* directionLabel(util::Direction direction);

    std::vector<ProgramCard> m_availableCards;
    std::optional<ProgramCard> m_selectedCard;
    std::optional<util::Direction> m_pendingDirection;
    int m_selectedCardIndex = -1;
    int m_hoveredCardIndex = -1;
    int m_cardScrollOffset = 0;
    int m_equipmentScrollOffset = 0;
    bool m_hoveringConfirm = false;
    bool m_confirmed = false;
    int m_playerHp = 0, m_playerMaxHp = 1, m_playerHeat = 0, m_playerMaxHeat = 1;
    std::string m_weaponName;
    std::string m_weaponDescription;
    std::vector<std::string> m_combatLog;
    std::vector<EquipmentEntry> m_equipmentEntries;
    int m_hoveredEquipmentIndex = -1;
    bool m_hoveringEquipmentScrollBack = false;
    bool m_hoveringEquipmentScrollForward = false;
    std::optional<int> m_requestedWeaponSlot;
    int m_weaponCycleRequest = 0;
    std::string m_hoverTooltip;
    std::string m_combatLogText;
    std::string m_worldHoverText;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
};

#endif // IRON_CIRCUIT_VIEW_HUMANVIEW_H
