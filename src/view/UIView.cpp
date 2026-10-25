#include "view/UIView.h"
#include "model/GameModel.h"
#include "render/DrawQueue.h"
#include "util/GameConfig.h"
#include "util/Types.h"
#include <SDL_events.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace {
    enum UIState { UI_NONE = 0, UI_TITLE = 1, UI_LOADOUT = 2, UI_REWARD = 3, UI_INTERMISSION = 4 };
    constexpr int kVisibleComponentCount = 6;

    struct RectBox {
        int x;
        int y;
        int w;
        int h;
    };

    RectBox primaryButtonRect() {
        return RectBox{util::layout::LogicalWidth - 380, 746, 220, 58};
    }

    RectBox titleButtonRect() {
        return RectBox{util::layout::LogicalWidth / 2 - 140, 620, 280, 64};
    }

    bool pointInRect(int px, int py, const RectBox& r) {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    }

    RectBox weaponRect(int index) {
        const int panelW = 340;
        const int panelH = 132;
        const int pad = 24;
        const int totalW = 3 * panelW + 2 * pad;
        const int startX = (util::layout::LogicalWidth - totalW) / 2;
        return RectBox{startX + index * (panelW + pad), 188, panelW, panelH};
    }

    RectBox componentRect(int index) {
        const int cols = 3;
        const int panelW = 330;
        const int panelH = 108;
        const int padX = 22;
        const int padY = 20;
        const int totalW = cols * panelW + (cols - 1) * padX;
        const int startX = (util::layout::LogicalWidth - totalW) / 2;
        const int row = index / cols;
        const int col = index % cols;
        return RectBox{
            startX + col * (panelW + padX),
            430 + row * (panelH + padY),
            panelW,
            panelH
        };
    }

    RectBox componentScrollBackRect() {
        return RectBox{1050, 370, 52, 42};
    }

    RectBox componentScrollForwardRect() {
        return RectBox{1114, 370, 52, 42};
    }

    RectBox rewardChoiceRect(int row, int index) {
        const int panelX = 180;
        const int panelY = 270;
        const int panelW = util::layout::LogicalWidth - 360;
        const int rowTop = panelY + row * 160;
        const int cellGap = 14;
        const int cellW = (panelW - cellGap * 4) / 3;
        const int cellH = 56;
        return RectBox{panelX + cellGap + index * (cellW + cellGap), rowTop + 52, cellW, cellH};
    }

    std::string titleFromId(const std::string& id) {
        std::string out;
        bool upper = true;
        for (char c : id) {
            if (c == '_' || c == '-') { out.push_back(' '); upper = true; continue; }
            out.push_back(upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c);
            upper = false;
        }
        return out;
    }

    int maxComponentScrollOffset(int totalCount) {
        return std::max(0, totalCount - kVisibleComponentCount);
    }
}

UIView::UIView(GameModel* model) : m_model(model) {
    m_loadoutWeaponIds = {"sword", "laser", "arc_blade"};
    m_loadoutWeaponNames = {"Sword", "Laser", "Arc Blade"};
    m_loadoutWeaponTags = {"MELEE", "RANGED", "MELEE"};
    m_loadoutWeaponDetails = {
        "Reliable front strike.",
        "Piercing line pressure.",
        "Short-range fan cleave."
    };
    m_loadoutItemIds = {"lifesteal", "reactive_armor", "time_slow", "overclock", "blink"};
    m_loadoutItemNames = {"Lifesteal", "Reactive Armor", "Time Slow", "Overclock", "Blink"};
    m_loadoutItemTags = {"ITEM", "ITEM", "ITEM", "ITEM", "ITEM"};
    m_loadoutItemDetails = {
        "Heal from dealt damage.",
        "Reflect part of incoming hits.",
        "Reduce enemy action cadence.",
        "Boost damage at self-cost.",
        "Teleport forward on blocked movement."
    };

    std::ifstream in("resource/config/content_catalog.json");
    if (!in.is_open()) return;
    nlohmann::json j;
    try { in >> j; } catch (...) { return; }
    if (j.contains("weapons") && j["weapons"].is_array()) {
        m_loadoutWeaponIds.clear();
        m_loadoutWeaponNames.clear();
        m_loadoutWeaponTags.clear();
        m_loadoutWeaponDetails.clear();
        for (const auto& w : j["weapons"]) {
            const std::string id = w.value("id", "");
            if (id.empty()) continue;
            m_loadoutWeaponIds.push_back(id);
            m_loadoutWeaponNames.push_back(titleFromId(id));
            m_loadoutWeaponTags.push_back("CATALOG");
            const double cooldown = w.value("cooldown", 0.0);
            m_loadoutWeaponDetails.push_back("Cooldown " + std::to_string(cooldown) + " | " + w.value("range", std::string("n/a")));
            if (static_cast<int>(m_loadoutWeaponIds.size()) >= 3) break;
        }
    }
    if (j.contains("items") && j["items"].is_array()) {
        m_loadoutItemIds.clear();
        m_loadoutItemNames.clear();
        m_loadoutItemTags.clear();
        m_loadoutItemDetails.clear();
        for (const auto& it : j["items"]) {
            const std::string id = it.value("id", "");
            if (id.empty()) continue;
            m_loadoutItemIds.push_back(id);
            m_loadoutItemNames.push_back(titleFromId(id));
            m_loadoutItemTags.push_back("ITEM");
            m_loadoutItemDetails.push_back("Prototype item from content catalog.");
        }
    }
    // Add a few weapons into the "pick 3" pool so selection can mix items + weapon modules.
    if (j.contains("weapons") && j["weapons"].is_array()) {
        int added = 0;
        for (const auto& w : j["weapons"]) {
            const std::string id = w.value("id", "");
            if (id.empty()) continue;
            m_loadoutItemIds.push_back("weapon:" + id);
            m_loadoutItemNames.push_back(titleFromId(id));
            m_loadoutItemTags.push_back("WEAPON MOD");
            m_loadoutItemDetails.push_back("Adds weapon-themed module behavior from catalog.");
            ++added;
            if (added >= 3) break;
        }
    }
}

void UIView::setState(int state) {
    m_state = state;
    if (m_state == UI_LOADOUT) {
        m_componentScrollOffset = std::clamp(m_componentScrollOffset, 0, maxComponentScrollOffset(static_cast<int>(m_loadoutItemIds.size())));
    }
    if (m_state == UI_NONE) {
        m_hoveredWeaponChoice.reset();
        m_hoveredComponentChoice.reset();
        m_hoveredMoveSlotChoice.reset();
        m_hoveringButton = false;
        m_hoveringComponentScrollBack = false;
        m_hoveringComponentScrollForward = false;
        m_continueChosen = false;
        m_loadoutConfirmed = false;
        m_titleStartChosen = false;
        m_rewardContinueChosen = false;
        m_intermissionTitle.clear();
        m_intermissionBody.clear();
        m_intermissionButton = "Continue";
    }
}

void UIView::showTitleScreen() {
    m_state = UI_TITLE;
    m_titleStartChosen = false;
    m_hoveringButton = false;
    m_intermissionTitle = "Iron Circuit";
    m_intermissionBody = "You are a robot trying to survive without being assimilated by the core AI. Clear out rooms of bots trying to turn you into scrap and slowly build yourself stronger with new components and weapons!";
    m_intermissionButton = "Start Game";
}

void UIView::showIntermission(std::string title, std::string body, std::string buttonLabel) {
    m_state = UI_INTERMISSION;
    m_intermissionTitle = std::move(title);
    m_intermissionBody = std::move(body);
    m_intermissionButton = std::move(buttonLabel);
    m_continueChosen = false;
    m_hoveringButton = false;
}

void UIView::showRewardScreen(std::string title,
                              std::string body,
                              std::vector<std::string> weaponChoices,
                              std::vector<std::string> weaponDetails,
                              std::vector<std::string> componentChoices,
                              std::vector<std::string> componentDetails,
                              std::vector<std::string> moveChoices,
                              std::vector<std::string> moveDetails,
                              std::string buttonLabel) {
    m_state = UI_REWARD;
    m_intermissionTitle = std::move(title);
    m_intermissionBody = std::move(body);
    m_intermissionButton = std::move(buttonLabel);
    m_rewardWeaponChoices = std::move(weaponChoices);
    m_rewardWeaponDetails = std::move(weaponDetails);
    m_rewardComponentChoices = std::move(componentChoices);
    m_rewardComponentDetails = std::move(componentDetails);
    m_rewardMoveChoices = std::move(moveChoices);
    m_rewardMoveDetails = std::move(moveDetails);
    m_selectedRewardWeapon.reset();
    m_selectedRewardComponent.reset();
    m_selectedRewardMoves.clear();
    m_rewardContinueChosen = false;
    m_hoveredWeaponChoice.reset();
    m_hoveredComponentChoice.reset();
    m_hoveredMoveSlotChoice.reset();
    m_hoveringButton = false;
}

std::optional<int> UIView::hoveredWeaponChoice(int mx, int my) const {
    for (int i = 0; i < static_cast<int>(m_loadoutWeaponIds.size()); ++i) {
        if (pointInRect(mx, my, weaponRect(i))) return i;
    }
    return std::nullopt;
}

std::optional<int> UIView::hoveredComponentChoice(int mx, int my) const {
    const int visibleCount = std::min(kVisibleComponentCount, static_cast<int>(m_loadoutItemIds.size()) - m_componentScrollOffset);
    for (int visibleIndex = 0; visibleIndex < visibleCount; ++visibleIndex) {
        if (pointInRect(mx, my, componentRect(visibleIndex))) {
            return m_componentScrollOffset + visibleIndex;
        }
    }
    return std::nullopt;
}

std::optional<int> UIView::hoveredRewardWeaponChoice(int mx, int my) const {
    for (int i = 0; i < static_cast<int>(m_rewardWeaponChoices.size()); ++i) {
        if (pointInRect(mx, my, rewardChoiceRect(0, i))) return i;
    }
    return std::nullopt;
}

std::optional<int> UIView::hoveredRewardComponentChoice(int mx, int my) const {
    for (int i = 0; i < static_cast<int>(m_rewardComponentChoices.size()); ++i) {
        if (pointInRect(mx, my, rewardChoiceRect(1, i))) return i;
    }
    return std::nullopt;
}

std::optional<int> UIView::hoveredRewardMoveChoice(int mx, int my) const {
    for (int i = 0; i < static_cast<int>(m_rewardMoveChoices.size()); ++i) {
        if (pointInRect(mx, my, rewardChoiceRect(2, i))) return i;
    }
    return std::nullopt;
}

bool UIView::hoveringPrimaryButton(int mx, int my) const {
    if (m_state == UI_TITLE) return pointInRect(mx, my, titleButtonRect());
    return pointInRect(mx, my, primaryButtonRect());
}

bool UIView::hoveringComponentScrollBackButton(int mx, int my) const {
    return m_state == UI_LOADOUT && pointInRect(mx, my, componentScrollBackRect());
}

bool UIView::hoveringComponentScrollForwardButton(int mx, int my) const {
    return m_state == UI_LOADOUT && pointInRect(mx, my, componentScrollForwardRect());
}

void UIView::handleSDLEvent(const SDL_Event& event) {
    if (m_state == UI_NONE) {
        (void)event;
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        if (m_state == UI_LOADOUT) {
            m_hoveredWeaponChoice = hoveredWeaponChoice(event.motion.x, event.motion.y);
            m_hoveredComponentChoice = hoveredComponentChoice(event.motion.x, event.motion.y);
            m_hoveringComponentScrollBack = hoveringComponentScrollBackButton(event.motion.x, event.motion.y);
            m_hoveringComponentScrollForward = hoveringComponentScrollForwardButton(event.motion.x, event.motion.y);
        } else if (m_state == UI_REWARD) {
            m_hoveredWeaponChoice = hoveredRewardWeaponChoice(event.motion.x, event.motion.y);
            m_hoveredComponentChoice = hoveredRewardComponentChoice(event.motion.x, event.motion.y);
            m_hoveredMoveSlotChoice = hoveredRewardMoveChoice(event.motion.x, event.motion.y);
            m_hoveringComponentScrollBack = false;
            m_hoveringComponentScrollForward = false;
        }
        m_hoveringButton = hoveringPrimaryButton(event.motion.x, event.motion.y);
        return;
    }

    if (event.type != SDL_MOUSEBUTTONDOWN || event.button.button != SDL_BUTTON_LEFT) return;

    if (m_state == UI_TITLE) {
        if (hoveringPrimaryButton(event.button.x, event.button.y)) {
            m_titleStartChosen = true;
        }
        return;
    }

    if (m_state == UI_LOADOUT) {
        if (hoveringComponentScrollBackButton(event.button.x, event.button.y)) {
            m_componentScrollOffset = std::max(0, m_componentScrollOffset - 1);
            m_hoveredComponentChoice.reset();
            return;
        }

        if (hoveringComponentScrollForwardButton(event.button.x, event.button.y)) {
            m_componentScrollOffset = std::min(maxComponentScrollOffset(static_cast<int>(m_loadoutItemIds.size())), m_componentScrollOffset + 1);
            m_hoveredComponentChoice.reset();
            return;
        }

        if (auto hoveredWeapon = hoveredWeaponChoice(event.button.x, event.button.y)) {
            m_loadoutWeaponChoice = hoveredWeapon;
            return;
        }

        if (auto hoveredComponent = hoveredComponentChoice(event.button.x, event.button.y)) {
            auto existing = std::find(m_selectedComponentChoices.begin(), m_selectedComponentChoices.end(), *hoveredComponent);
            if (existing != m_selectedComponentChoices.end()) {
                m_selectedComponentChoices.erase(existing);
            } else if (m_selectedComponentChoices.size() < 3) {
                m_selectedComponentChoices.push_back(*hoveredComponent);
            }
            return;
        }

        if (hoveringPrimaryButton(event.button.x, event.button.y) && m_loadoutWeaponChoice.has_value() && m_selectedComponentChoices.size() == 3) {
            m_loadoutConfirmed = true;
        }
        return;
    }

    if (m_state == UI_REWARD) {
        if (auto hoveredWeapon = hoveredRewardWeaponChoice(event.button.x, event.button.y)) {
            m_selectedRewardWeapon = hoveredWeapon;
            return;
        }
        if (auto hoveredComponent = hoveredRewardComponentChoice(event.button.x, event.button.y)) {
            m_selectedRewardComponent = hoveredComponent;
            return;
        }
        if (auto hoveredMove = hoveredRewardMoveChoice(event.button.x, event.button.y)) {
            // Only allow selecting exactly 1 move - clicking a different move replaces the selection
            m_selectedRewardMoves.clear();
            m_selectedRewardMoves.push_back(*hoveredMove);
            return;
        }
        if (hoveringPrimaryButton(event.button.x, event.button.y)) {
            m_rewardContinueChosen = true;
        }
        return;
    }

    if (m_state == UI_INTERMISSION && hoveringPrimaryButton(event.button.x, event.button.y)) {
        m_continueChosen = true;
    }
}

std::optional<int> UIView::getUpgradeChoice() const {
    return m_upgradeChoice;
}

bool UIView::consumeTitleStart() {
    const bool chosen = m_titleStartChosen;
    m_titleStartChosen = false;
    return chosen;
}

bool UIView::consumeLoadoutConfirmed() {
    const bool confirmed = m_loadoutConfirmed;
    m_loadoutConfirmed = false;
    return confirmed;
}

bool UIView::consumeRewardContinue() {
    const bool chosen = m_rewardContinueChosen;
    m_rewardContinueChosen = false;
    return chosen;
}

bool UIView::consumeContinueChoice() {
    const bool chosen = m_continueChosen;
    m_continueChosen = false;
    return chosen;
}

void UIView::clearLoadoutSelections() {
    m_loadoutWeaponChoice.reset();
    m_hoveredWeaponChoice.reset();
    m_hoveredComponentChoice.reset();
    m_hoveringComponentScrollBack = false;
    m_hoveringComponentScrollForward = false;
    m_selectedComponentChoices.clear();
    m_componentScrollOffset = 0;
    m_loadoutConfirmed = false;
}

void UIView::clearRewardSelections() {
    m_selectedRewardWeapon.reset();
    m_selectedRewardComponent.reset();
    m_selectedRewardMoves.clear();
    m_hoveredWeaponChoice.reset();
    m_hoveredComponentChoice.reset();
    m_hoveredMoveSlotChoice.reset();
    m_rewardContinueChosen = false;
}

void UIView::clearChoices() {
    m_upgradeChoice.reset();
    clearLoadoutSelections();
    clearRewardSelections();
    m_hoveringButton = false;
    m_continueChosen = false;
    m_titleStartChosen = false;
}

void UIView::submitDrawRequests(DrawQueue* queue) {
    if (!queue || m_state == UI_NONE) return;

    DrawRequest dim;
    dim.layer = util::DrawLayer::UI_OVERLAY;
    dim.usePlaceholderShape = true;
    dim.placeholderColor = 0xC0000000u;
    dim.dst = {0, 0, util::layout::LogicalWidth, util::layout::LogicalHeight};
    dim.zOrder = 3000;
    queue->push(dim);

    DrawRequest backdrop;
    backdrop.layer = util::DrawLayer::UI_OVERLAY;
    backdrop.usePlaceholderShape = true;
    backdrop.placeholderColor = 0xEE121722u;
    backdrop.dst = {96, 88, util::layout::LogicalWidth - 192, util::layout::LogicalHeight - 176};
    backdrop.zOrder = 3005;
    queue->push(backdrop);

    if (m_state == UI_TITLE) {
        DrawRequest imageFrame;
        imageFrame.layer = util::DrawLayer::UI_OVERLAY;
        imageFrame.usePlaceholderShape = true;
        imageFrame.placeholderColor = 0xFF1A2230u;
        imageFrame.dst = {300, 156, 1000, 360};
        imageFrame.zOrder = 3008;
        queue->push(imageFrame);

        DrawRequest heroImage;
        heroImage.layer = util::DrawLayer::UI_OVERLAY;
        heroImage.texturePath = "resource/image/StartScreen.png";
        heroImage.dst = {312, 168, 976, 336};
        heroImage.zOrder = 3009;
        queue->push(heroImage);

        DrawRequest title;
        title.layer = util::DrawLayer::UI_OVERLAY;
        title.hasText = true;
        title.textColor = 0xFFFFFFFFu;
        title.text = m_intermissionTitle;
        title.textPointSize = 34;
        title.dst = {util::layout::LogicalWidth / 2 - 260, 108, 520, 52};
        title.zOrder = 3012;
        queue->push(title);

        DrawRequest body;
        body.layer = util::DrawLayer::UI_OVERLAY;
        body.hasText = true;
        body.textColor = 0xFFE8E8E8u;
        body.text = m_intermissionBody;
        body.dst = {236, 526, util::layout::LogicalWidth - 472, 88};
        body.zOrder = 3013;
        queue->push(body);

        RectBox button = titleButtonRect();
        DrawRequest buttonBg;
        buttonBg.layer = util::DrawLayer::UI_OVERLAY;
        buttonBg.usePlaceholderShape = true;
        buttonBg.placeholderColor = m_hoveringButton ? 0xFF2CBF6Du : 0xFF1F9F55u;
        buttonBg.dst = {button.x, button.y, button.w, button.h};
        buttonBg.zOrder = 3020;
        queue->push(buttonBg);

        DrawRequest buttonText;
        buttonText.layer = util::DrawLayer::UI_OVERLAY;
        buttonText.hasText = true;
        buttonText.textColor = 0xFFFFFFFFu;
        buttonText.text = m_intermissionButton;
        buttonText.dst = {button.x + 24, button.y + 18, button.w - 48, 28};
        buttonText.zOrder = 3021;
        queue->push(buttonText);
        return;
    }

    if (m_state == UI_LOADOUT) {
        DrawRequest title;
        title.layer = util::DrawLayer::UI_OVERLAY;
        title.hasText = true;
        title.textColor = 0xFFFFFFFFu;
        title.text = "Choose 1 weapon and 3 support modules";
        title.dst = {148, 122, 1100, 42};
        title.zOrder = 3010;
        queue->push(title);

        DrawRequest subtitle;
        subtitle.layer = util::DrawLayer::UI_OVERLAY;
        subtitle.hasText = true;
        subtitle.textColor = 0xFFD8D8D8u;
        subtitle.text = "Each module adds a move card plus a passive stat bonus. Select exactly three before launching the level.";
        subtitle.dst = {148, 164, 1180, 48};
        subtitle.zOrder = 3010;
        queue->push(subtitle);

        for (int i = 0; i < static_cast<int>(m_loadoutWeaponIds.size()); ++i) {
            const RectBox box = weaponRect(i);
            const bool selected = (m_loadoutWeaponChoice && *m_loadoutWeaponChoice == i);
            const bool hovered = (m_hoveredWeaponChoice && *m_hoveredWeaponChoice == i);

            DrawRequest panel;
            panel.layer = util::DrawLayer::UI_OVERLAY;
            panel.usePlaceholderShape = true;
            panel.placeholderColor = selected ? 0xFF208020u : (hovered ? 0xFF50617Eu : 0xFF374156u);
            panel.dst = {box.x, box.y, box.w, box.h};
            panel.zOrder = 3020 + i;
            queue->push(panel);

            if (hovered || selected) {
                DrawRequest outline;
                outline.layer = util::DrawLayer::UI_OVERLAY;
                outline.usePlaceholderShape = true;
                outline.outlineOnly = true;
                outline.placeholderColor = selected ? 0xFFFFE08Au : 0xFFFFFFFFu;
                outline.dst = {box.x - 3, box.y - 3, box.w + 6, box.h + 6};
                outline.zOrder = 3025 + i;
                queue->push(outline);
            }

            DrawRequest label;
            label.layer = util::DrawLayer::UI_OVERLAY;
            label.hasText = true;
            label.textColor = 0xFFFFFFFFu;
            label.text = m_loadoutWeaponNames[static_cast<size_t>(i)] + " (" + m_loadoutWeaponTags[static_cast<size_t>(i)] + ")";
            label.dst = {box.x + 18, box.y + 14, box.w - 36, 26};
            label.zOrder = 3030 + i;
            queue->push(label);

            DrawRequest tag;
            tag.layer = util::DrawLayer::UI_OVERLAY;
            tag.hasText = true;
            tag.textColor = 0xFFFFE08Au;
            tag.text = m_loadoutWeaponTags[static_cast<size_t>(i)];
            tag.dst = {box.x + 18, box.y + 48, box.w - 36, 24};
            tag.zOrder = 3031 + i;
            queue->push(tag);

            DrawRequest detail;
            detail.layer = util::DrawLayer::UI_OVERLAY;
            detail.hasText = true;
            detail.textColor = 0xFFE8E8E8u;
            detail.text = m_loadoutWeaponDetails[static_cast<size_t>(i)];
            detail.dst = {box.x + 18, box.y + 78, box.w - 36, 42};
            detail.zOrder = 3032 + i;
            queue->push(detail);
        }

        DrawRequest compTitle;
        compTitle.layer = util::DrawLayer::UI_OVERLAY;
        compTitle.hasText = true;
        compTitle.textColor = 0xFFFFFFFFu;
        compTitle.text = "Loadout Items";
        compTitle.dst = {148, 378, 420, 32};
        compTitle.zOrder = 3040;
        queue->push(compTitle);

        DrawRequest compHint;
        compHint.layer = util::DrawLayer::UI_OVERLAY;
        compHint.hasText = true;
        compHint.textColor = 0xFFD8D8D8u;
        compHint.text = "Use the on-screen arrows to browse the full module list.";
        compHint.dst = {590, 378, 360, 28};
        compHint.zOrder = 3040;
        queue->push(compHint);

        const bool canScrollBack = m_componentScrollOffset > 0;
        const bool canScrollForward = (m_componentScrollOffset + kVisibleComponentCount) < static_cast<int>(m_loadoutItemIds.size());
        const RectBox backBox = componentScrollBackRect();
        const RectBox forwardBox = componentScrollForwardRect();

        DrawRequest backButton;
        backButton.layer = util::DrawLayer::UI_OVERLAY;
        backButton.usePlaceholderShape = true;
        backButton.placeholderColor = canScrollBack
            ? (m_hoveringComponentScrollBack ? 0xFF5B7FB3u : 0xFF42597Eu)
            : 0xFF2D3646u;
        backButton.dst = {backBox.x, backBox.y, backBox.w, backBox.h};
        backButton.zOrder = 3041;
        queue->push(backButton);

        DrawRequest backLabel;
        backLabel.layer = util::DrawLayer::UI_OVERLAY;
        backLabel.hasText = true;
        backLabel.textColor = canScrollBack ? 0xFFFFFFFFu : 0xFF9DA8B8u;
        backLabel.text = "<";
        backLabel.dst = {backBox.x + 18, backBox.y + 8, backBox.w - 36, 24};
        backLabel.zOrder = 3042;
        queue->push(backLabel);

        DrawRequest forwardButton;
        forwardButton.layer = util::DrawLayer::UI_OVERLAY;
        forwardButton.usePlaceholderShape = true;
        forwardButton.placeholderColor = canScrollForward
            ? (m_hoveringComponentScrollForward ? 0xFF5B7FB3u : 0xFF42597Eu)
            : 0xFF2D3646u;
        forwardButton.dst = {forwardBox.x, forwardBox.y, forwardBox.w, forwardBox.h};
        forwardButton.zOrder = 3043;
        queue->push(forwardButton);

        DrawRequest forwardLabel;
        forwardLabel.layer = util::DrawLayer::UI_OVERLAY;
        forwardLabel.hasText = true;
        forwardLabel.textColor = canScrollForward ? 0xFFFFFFFFu : 0xFF9DA8B8u;
        forwardLabel.text = ">";
        forwardLabel.dst = {forwardBox.x + 18, forwardBox.y + 8, forwardBox.w - 36, 24};
        forwardLabel.zOrder = 3044;
        queue->push(forwardLabel);

        const int visibleCount = std::min(kVisibleComponentCount, static_cast<int>(m_loadoutItemIds.size()) - m_componentScrollOffset);
        for (int visibleIndex = 0; visibleIndex < visibleCount; ++visibleIndex) {
            const int i = m_componentScrollOffset + visibleIndex;
            const RectBox box = componentRect(visibleIndex);
            const bool selected = std::find(m_selectedComponentChoices.begin(), m_selectedComponentChoices.end(), i) != m_selectedComponentChoices.end();
            const bool hovered = (m_hoveredComponentChoice && *m_hoveredComponentChoice == i);

            DrawRequest panel;
            panel.layer = util::DrawLayer::UI_OVERLAY;
            panel.usePlaceholderShape = true;
            panel.placeholderColor = selected ? 0xFF2E7BBFu : (hovered ? 0xFF466995u : 0xFF2A3448u);
            panel.dst = {box.x, box.y, box.w, box.h};
            panel.zOrder = 3050 + visibleIndex;
            queue->push(panel);

            if (hovered || selected) {
                DrawRequest outline;
                outline.layer = util::DrawLayer::UI_OVERLAY;
                outline.usePlaceholderShape = true;
                outline.outlineOnly = true;
                outline.placeholderColor = selected ? 0xFF9FD8FFu : 0xFFFFFFFFu;
                outline.dst = {box.x - 3, box.y - 3, box.w + 6, box.h + 6};
                outline.zOrder = 3055 + visibleIndex;
                queue->push(outline);
            }

            DrawRequest name;
            name.layer = util::DrawLayer::UI_OVERLAY;
            name.hasText = true;
            name.textColor = 0xFFFFFFFFu;
            name.text = m_loadoutItemNames[static_cast<size_t>(i)];
            name.dst = {box.x + 14, box.y + 12, box.w - 28, 22};
            name.zOrder = 3060 + visibleIndex;
            queue->push(name);

            DrawRequest tag;
            tag.layer = util::DrawLayer::UI_OVERLAY;
            tag.hasText = true;
            tag.textColor = 0xFFFFE08Au;
            tag.text = m_loadoutItemTags[static_cast<size_t>(i)];
            tag.dst = {box.x + 14, box.y + 40, box.w - 28, 22};
            tag.zOrder = 3061 + visibleIndex;
            queue->push(tag);

            DrawRequest detail;
            detail.layer = util::DrawLayer::UI_OVERLAY;
            detail.hasText = true;
            detail.textColor = 0xFFE8E8E8u;
            detail.text = m_loadoutItemDetails[static_cast<size_t>(i)];
            detail.dst = {box.x + 14, box.y + 66, box.w - 28, 30};
            detail.zOrder = 3062 + visibleIndex;
            queue->push(detail);
        }

        DrawRequest summary;
        summary.layer = util::DrawLayer::UI_OVERLAY;
        summary.hasText = true;
        summary.textColor = 0xFFD8FFD8u;
        const int visibleStart = m_loadoutItemIds.empty() ? 0 : (m_componentScrollOffset + 1);
        const int visibleEnd = std::min(static_cast<int>(m_loadoutItemIds.size()), m_componentScrollOffset + kVisibleComponentCount);
        summary.text = "Selected items: " + std::to_string(m_selectedComponentChoices.size()) + " / 3    Viewing " +
                       std::to_string(visibleStart) + "-" + std::to_string(visibleEnd) + " of " +
                       std::to_string(m_loadoutItemIds.size());
        summary.dst = {148, 694, 560, 28};
        summary.zOrder = 3070;
        queue->push(summary);

        const RectBox button = primaryButtonRect();
        const bool ready = m_loadoutWeaponChoice.has_value() && m_selectedComponentChoices.size() == 3;
        DrawRequest buttonBg;
        buttonBg.layer = util::DrawLayer::UI_OVERLAY;
        buttonBg.usePlaceholderShape = true;
        buttonBg.placeholderColor = ready ? (m_hoveringButton ? 0xFF2CBF6Du : 0xFF1F9F55u) : 0xFF4B5568u;
        buttonBg.dst = {button.x, button.y, button.w, button.h};
        buttonBg.zOrder = 3080;
        queue->push(buttonBg);

        DrawRequest buttonText;
        buttonText.layer = util::DrawLayer::UI_OVERLAY;
        buttonText.hasText = true;
        buttonText.textColor = 0xFFFFFFFFu;
        buttonText.text = ready ? "Begin Encounter" : "Pick 1 weapon + 3 items";
        buttonText.dst = {button.x + 14, button.y + 16, button.w - 28, 26};
        buttonText.zOrder = 3081;
        queue->push(buttonText);
        return;
    }

    if (m_state == UI_REWARD) {
        DrawRequest title;
        title.layer = util::DrawLayer::UI_OVERLAY;
        title.hasText = true;
        title.textColor = 0xFFFFFFFFu;
        title.text = m_intermissionTitle;
        title.dst = {140, 122, 1000, 42};
        title.zOrder = 3010;
        queue->push(title);

        DrawRequest body;
        body.layer = util::DrawLayer::UI_OVERLAY;
        body.hasText = true;
        body.textColor = 0xFFE8E8E8u;
        body.text = m_intermissionBody;
        body.dst = {140, 164, 1180, 64};
        body.zOrder = 3011;
        queue->push(body);

        const char* rowTitles[3] = {"Weapon Choice", "Component Choice", "Move Choice"};
        for (int row = 0; row < 3; ++row) {
            DrawRequest rowTitle;
            rowTitle.layer = util::DrawLayer::UI_OVERLAY;
            rowTitle.hasText = true;
            rowTitle.textColor = 0xFFFFFFFFu;
            rowTitle.text = rowTitles[row];
            rowTitle.dst = {194, 284 + row * 160, 340, 26};
            rowTitle.zOrder = 3020 + row * 100;
            queue->push(rowTitle);
        }

        auto drawRewardRow = [&](int row,
                                 const std::vector<std::string>& choices,
                                 const std::vector<std::string>& details,
                                 const std::optional<int>& hovered,
                                 const std::optional<int>& selected,
                                 int zBase) {
            for (int i = 0; i < static_cast<int>(choices.size()); ++i) {
                const RectBox box = rewardChoiceRect(row, i);
                const bool isHovered = hovered && (*hovered == i);
                const bool isSelected = selected && (*selected == i);

                DrawRequest panel;
                panel.layer = util::DrawLayer::UI_OVERLAY;
                panel.usePlaceholderShape = true;
                panel.placeholderColor = isSelected ? 0xFF1F9F55u : (isHovered ? 0xFF50617Eu : 0xFF2A3448u);
                panel.dst = {box.x, box.y, box.w, box.h};
                panel.zOrder = zBase + i * 3;
                queue->push(panel);

                DrawRequest text;
                text.layer = util::DrawLayer::UI_OVERLAY;
                text.hasText = true;
                text.textColor = 0xFFFFFFFFu;
                text.text = choices[static_cast<size_t>(i)];
                text.dst = {box.x + 10, box.y + 14, box.w - 20, 28};
                text.zOrder = zBase + i * 3 + 1;
                queue->push(text);

                if (isHovered && i < static_cast<int>(details.size())) {
                    DrawRequest tip;
                    tip.layer = util::DrawLayer::UI_OVERLAY;
                    tip.hasText = true;
                    tip.textColor = 0xFFFFE08Au;
                    tip.text = details[static_cast<size_t>(i)];
                    tip.dst = {194, box.y + 64, util::layout::LogicalWidth - 388, 56};
                    tip.zOrder = zBase + i * 3 + 2;
                    queue->push(tip);
                }
            }
        };

        drawRewardRow(0, m_rewardWeaponChoices, m_rewardWeaponDetails, m_hoveredWeaponChoice, m_selectedRewardWeapon, 3040);
        drawRewardRow(1, m_rewardComponentChoices, m_rewardComponentDetails, m_hoveredComponentChoice, m_selectedRewardComponent, 3140);
        auto drawRewardRowMultiple = [&](int row,
                                        const std::vector<std::string>& choices,
                                        const std::vector<std::string>& details,
                                        const std::optional<int>& hovered,
                                        const std::vector<int>& selected,
                                        int zBase) {
            for (int i = 0; i < static_cast<int>(choices.size()); ++i) {
                const RectBox box = rewardChoiceRect(row, i);
                const bool isHovered = hovered && (*hovered == i);
                const bool isSelected = std::find(selected.begin(), selected.end(), i) != selected.end();

                DrawRequest panel;
                panel.layer = util::DrawLayer::UI_OVERLAY;
                panel.usePlaceholderShape = true;
                panel.placeholderColor = isSelected ? 0xFF1F9F55u : (isHovered ? 0xFF50617Eu : 0xFF2A3448u);
                panel.dst = {box.x, box.y, box.w, box.h};
                panel.zOrder = zBase + i * 3;
                queue->push(panel);

                DrawRequest text;
                text.layer = util::DrawLayer::UI_OVERLAY;
                text.hasText = true;
                text.textColor = 0xFFFFFFFFu;
                text.text = choices[static_cast<size_t>(i)];
                text.dst = {box.x + 10, box.y + 14, box.w - 20, 28};
                text.zOrder = zBase + i * 3 + 1;
                queue->push(text);

                if (isHovered && i < static_cast<int>(details.size())) {
                    DrawRequest tip;
                    tip.layer = util::DrawLayer::UI_OVERLAY;
                    tip.hasText = true;
                    tip.textColor = 0xFFFFE08Au;
                    tip.text = details[static_cast<size_t>(i)];
                    tip.dst = {194, box.y + 64, util::layout::LogicalWidth - 388, 56};
                    tip.zOrder = zBase + i * 3 + 2;
                    queue->push(tip);
                }
            }
        };

        drawRewardRowMultiple(2, m_rewardMoveChoices, m_rewardMoveDetails, m_hoveredMoveSlotChoice, m_selectedRewardMoves, 3240);

        // Check if all three selections are made
        const bool allSelected = m_selectedRewardWeapon.has_value() && 
                                  m_selectedRewardComponent.has_value() && 
                                  !m_selectedRewardMoves.empty();

        DrawRequest selectionStatus;
        selectionStatus.layer = util::DrawLayer::UI_OVERLAY;
        selectionStatus.hasText = true;
        selectionStatus.textColor = allSelected ? 0xFFD8FFD8u : 0xFFFFD8D8u;
        std::string statusText = "Select: ";
        statusText += m_selectedRewardWeapon.has_value() ? "[Weapon✓] " : "[Weapon] ";
        statusText += m_selectedRewardComponent.has_value() ? "[Component✓] " : "[Component] ";
        statusText += !m_selectedRewardMoves.empty() ? "[Move✓]" : "[Move]";
        selectionStatus.text = statusText;
        selectionStatus.text = "Select: ";
        selectionStatus.text += m_selectedRewardWeapon.has_value() ? "[Weapon OK] " : "[Weapon] ";
        selectionStatus.text += m_selectedRewardComponent.has_value() ? "[Component OK] " : "[Component] ";
        selectionStatus.text += !m_selectedRewardMoves.empty() ? "[Move OK]" : "[Move]";
        selectionStatus.dst = {140, 694, 600, 28};
        selectionStatus.zOrder = 3075;
        queue->push(selectionStatus);

        const RectBox button = primaryButtonRect();
        DrawRequest buttonBg;
        buttonBg.layer = util::DrawLayer::UI_OVERLAY;
        buttonBg.usePlaceholderShape = true;
        buttonBg.placeholderColor = allSelected ? (m_hoveringButton ? 0xFF2CBF6Du : 0xFF1F9F55u) : 0xFF4B5568u;
        buttonBg.dst = {button.x, button.y, button.w, button.h};
        buttonBg.zOrder = 3080;
        queue->push(buttonBg);

        DrawRequest buttonText;
        buttonText.layer = util::DrawLayer::UI_OVERLAY;
        buttonText.hasText = true;
        buttonText.textColor = allSelected ? 0xFFFFFFFFu : 0xFF9DA8B8u;
        buttonText.text = allSelected ? m_intermissionButton : "Select 1 of each";
        buttonText.dst = {button.x + 14, button.y + 16, button.w - 28, 26};
        buttonText.zOrder = 3081;
        queue->push(buttonText);
        return;
    }

    DrawRequest title;
    title.layer = util::DrawLayer::UI_OVERLAY;
    title.hasText = true;
    title.textColor = 0xFFFFFFFFu;
    title.text = m_intermissionTitle.empty() ? "Level Clear" : m_intermissionTitle;
    title.dst = {200, 220, 1000, 48};
    title.zOrder = 3010;
    queue->push(title);

    DrawRequest body;
    body.layer = util::DrawLayer::UI_OVERLAY;
    body.hasText = true;
    body.textColor = 0xFFE8E8E8u;
    body.text = m_intermissionBody.empty() ? "Continue to the next level." : m_intermissionBody;
    body.dst = {200, 300, 1120, 120};
    body.zOrder = 3011;
    queue->push(body);

    const RectBox button = primaryButtonRect();
    DrawRequest buttonBg;
    buttonBg.layer = util::DrawLayer::UI_OVERLAY;
    buttonBg.usePlaceholderShape = true;
    buttonBg.placeholderColor = m_hoveringButton ? 0xFF2CBF6Du : 0xFF1F9F55u;
    buttonBg.dst = {button.x, button.y, button.w, button.h};
    buttonBg.zOrder = 3020;
    queue->push(buttonBg);

    DrawRequest buttonText;
    buttonText.layer = util::DrawLayer::UI_OVERLAY;
    buttonText.hasText = true;
    buttonText.textColor = 0xFFFFFFFFu;
    buttonText.text = m_intermissionButton;
    buttonText.dst = {button.x + 18, button.y + 16, button.w - 36, 24};
    buttonText.zOrder = 3021;
    queue->push(buttonText);
}

std::vector<std::string> UIView::getSelectedLoadoutItemIds() const {
    std::vector<std::string> out;
    for (int idx : m_selectedComponentChoices) {
        if (idx >= 0 && idx < static_cast<int>(m_loadoutItemIds.size())) {
            out.push_back(m_loadoutItemIds[static_cast<size_t>(idx)]);
        }
    }
    return out;
}
