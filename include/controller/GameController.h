#ifndef IRON_CIRCUIT_CONTROLLER_GAMECONTROLLER_H
#define IRON_CIRCUIT_CONTROLLER_GAMECONTROLLER_H

#include <memory>
#include <unordered_map>
#include "model/ProgramCard.h"
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
class GameModel;
class HumanView;
class UIView;
class HumanController;
class Renderer;
class PlayerRobot;
class Component;
class EnemyRobot;
class AudioManager;

class GameController {
public:
    GameController();
    ~GameController();

    void run();
    void setStartLevelIndex(int index) { m_startLevelIndex = index; }
    void setEnableLoadoutUI(bool enable) { m_enableLoadoutUI = enable; }

private:
    struct InventoryEntry {
        std::string label;
        std::string detail;
    };
    struct EnemyTurnAnimation {
        int enemyId = -1;
        util::Vector2i startPos{};
        util::Vector2i targetPos{};
        util::MoveType moveType = util::MoveType::NONE;
    };

    bool initSDL();
    void shutdownSDL();
    void pollEvents();
    void update();
    void render();
    void collectAndSubmitActions();
    void beginTurnAnimation();
    void updateTurnAnimation();
    void rebuildHumanController();
    void loadRunLevel(int levelIndex);
    void applyCurrentLoadout();
    void openRewardScreen(bool isInitialLoadout = false);
    void openDefeatScreen();
    void applyRewardSelections();
    void advanceAfterReward();
    void resetRunProgress(bool returnToTitleScreen);
    void skipCurrentLevelToReward();

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_sdlRenderer = nullptr;
    std::unique_ptr<GameModel> m_model;
    std::unique_ptr<HumanView> m_humanView;
    std::unique_ptr<UIView> m_uiView;
    std::unique_ptr<HumanController> m_humanController;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<AudioManager> m_audioManager;
    bool m_running = true;
    int m_startLevelIndex = 1;
    bool m_enableLoadoutUI = true;
    int m_runLevel = 1;
    int m_equippedWeaponChoice = 0;
    std::vector<int> m_ownedWeaponChoices;
    std::unordered_map<int, int> m_weaponSelectionRounds;
    std::vector<int> m_ownedSupportIds;
    std::vector<int> m_equippedSupportIds;
    std::vector<int> m_equippedMoveIds;
    std::vector<std::string> m_selectedItemIds;
    std::vector<ProgramCard> m_runMoveCards;
    std::vector<InventoryEntry> m_inventoryEntries;
    std::vector<int> m_pendingRewardWeaponChoices;
    std::vector<int> m_pendingRewardComponentChoices;
    std::vector<ProgramCard> m_pendingRewardMoveChoices;
    bool m_rewardScreenOpen = false;
    bool m_initialRewardSelectionOpen = false;
    bool m_defeatScreenOpen = false;
    bool m_turnAnimationActive = false;
    std::vector<util::Vector2i> m_turnAnimationPath;
    std::vector<EnemyTurnAnimation> m_enemyTurnAnimations;
    util::Vector2i m_turnAnimationStart{};
    util::Direction m_turnAnimationFacing = util::Direction::RIGHT;
    unsigned m_turnAnimationStartMs = 0;
    bool m_turnResolvedDuringAnimation = false;
    unsigned m_turnAnimationResolveMs = 0;
    static constexpr int kMaxRunLevels = 5;
};

#endif // IRON_CIRCUIT_CONTROLLER_GAMECONTROLLER_H
