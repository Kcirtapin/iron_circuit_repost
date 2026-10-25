#ifndef IRON_CIRCUIT_VIEW_UIVIEW_H
#define IRON_CIRCUIT_VIEW_UIVIEW_H

#include <optional>
#include <string>
#include <vector>
#include <SDL_events.h>

class GameModel;
class DrawQueue;

class UIView {
public:
    explicit UIView(GameModel* model);

    void setState(int state);
    void showTitleScreen();
    void showIntermission(std::string title, std::string body, std::string buttonLabel = "Continue");
    void showRewardScreen(std::string title,
                          std::string body,
                          std::vector<std::string> weaponChoices,
                          std::vector<std::string> weaponDetails,
                          std::vector<std::string> componentChoices,
                          std::vector<std::string> componentDetails,
                          std::vector<std::string> moveChoices,
                          std::vector<std::string> moveDetails,
                          std::string buttonLabel = "Continue");
    void handleSDLEvent(const SDL_Event& event);
    std::optional<int> getUpgradeChoice() const;
    std::optional<int> getLoadoutChoice() const { return m_loadoutWeaponChoice; }
    const std::vector<int>& getLoadoutComponentChoices() const { return m_selectedComponentChoices; }
    std::vector<std::string> getSelectedLoadoutItemIds() const;
    bool consumeTitleStart();
    bool consumeLoadoutConfirmed();
    bool consumeRewardContinue();
    bool consumeContinueChoice();
    std::optional<int> getSelectedRewardWeapon() const { return m_selectedRewardWeapon; }
    std::optional<int> getSelectedRewardComponent() const { return m_selectedRewardComponent; }
    std::vector<int> getSelectedRewardMoves() const { return m_selectedRewardMoves; }
    bool isBlocking() const { return m_state != 0; }
    void clearLoadoutSelections();
    void clearRewardSelections();
    void clearChoices();
    void submitDrawRequests(DrawQueue* queue);

private:
    std::optional<int> hoveredWeaponChoice(int mx, int my) const;
    std::optional<int> hoveredComponentChoice(int mx, int my) const;
    std::optional<int> hoveredRewardWeaponChoice(int mx, int my) const;
    std::optional<int> hoveredRewardComponentChoice(int mx, int my) const;
    std::optional<int> hoveredRewardMoveChoice(int mx, int my) const;
    bool hoveringPrimaryButton(int mx, int my) const;
    bool hoveringComponentScrollBackButton(int mx, int my) const;
    bool hoveringComponentScrollForwardButton(int mx, int my) const;
    GameModel* m_model = nullptr;
    int m_state = 0;
    std::optional<int> m_upgradeChoice;
    std::optional<int> m_loadoutWeaponChoice;
    std::optional<int> m_hoveredWeaponChoice;
    std::optional<int> m_hoveredComponentChoice;
    std::optional<int> m_hoveredMoveSlotChoice;
    std::vector<int> m_selectedComponentChoices;
    bool m_hoveringButton = false;
    bool m_continueChosen = false;
    bool m_loadoutConfirmed = false;
    bool m_titleStartChosen = false;
    bool m_rewardContinueChosen = false;
    bool m_hoveringComponentScrollBack = false;
    bool m_hoveringComponentScrollForward = false;
    std::optional<int> m_selectedRewardWeapon;
    std::optional<int> m_selectedRewardComponent;
    std::vector<int> m_selectedRewardMoves;
    std::string m_intermissionTitle;
    std::string m_intermissionBody;
    std::string m_intermissionButton = "Continue";
    std::vector<std::string> m_rewardWeaponChoices;
    std::vector<std::string> m_rewardWeaponDetails;
    std::vector<std::string> m_rewardComponentChoices;
    std::vector<std::string> m_rewardComponentDetails;
    std::vector<std::string> m_rewardMoveChoices;
    std::vector<std::string> m_rewardMoveDetails;
    std::vector<std::string> m_loadoutWeaponIds;
    std::vector<std::string> m_loadoutWeaponNames;
    std::vector<std::string> m_loadoutWeaponTags;
    std::vector<std::string> m_loadoutWeaponDetails;
    std::vector<std::string> m_loadoutItemIds;
    std::vector<std::string> m_loadoutItemNames;
    std::vector<std::string> m_loadoutItemTags;
    std::vector<std::string> m_loadoutItemDetails;
    int m_componentScrollOffset = 0;
};

#endif // IRON_CIRCUIT_VIEW_UIVIEW_H
