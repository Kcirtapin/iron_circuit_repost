#include "view/HumanView.h"
#include "render/DrawQueue.h"
#include "util/Types.h"
#include "util/GameConfig.h"
#include <SDL.h>
#include <algorithm>
#include <string>

namespace {
    constexpr int kSidebarPad = 24;
    constexpr int kCardPad = 18;
    constexpr int kCardWidth = 220;
    constexpr int kCardHeight = 64;
    constexpr int kEquipmentRowHeight = 20;

    struct SimpleRect {
        int x;
        int y;
        int w;
        int h;
    };

    SimpleRect equipmentBackButtonRect() {
        return SimpleRect{util::layout::SidebarX + util::layout::SidebarW - kSidebarPad - 64,
                          util::layout::SidebarY + 382, 26, 24};
    }

    SimpleRect equipmentForwardButtonRect() {
        return SimpleRect{util::layout::SidebarX + util::layout::SidebarW - kSidebarPad - 32,
                          util::layout::SidebarY + 382, 26, 24};
    }
}

HumanView::Rect HumanView::getCardRect(int index) const {
    const int x = util::layout::BottomBarX + kCardPad + index * (kCardWidth + kCardPad);
    const int y = util::layout::BottomBarY + 16;
    return Rect{x, y, kCardWidth, kCardHeight};
}

HumanView::Rect HumanView::getConfirmRect() const {
    const int w = 180;
    const int h = 56;
    const int x = util::layout::BottomBarX + util::layout::BottomBarW - w - 20;
    const int y = util::layout::BottomBarY + 18;
    return Rect{x, y, w, h};
}

HumanView::Rect HumanView::getEquipmentPanelRect() const {
    const int x = util::layout::SidebarX + kSidebarPad;
    const int w = util::layout::SidebarW - kSidebarPad * 2;
    return Rect{x, util::layout::SidebarY + 408, w, 124};
}

bool HumanView::pointInRect(int px, int py, const Rect& r) {
    return (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h);
}

const char* HumanView::directionLabel(util::Direction direction) {
    switch (direction) {
        case util::Direction::UP: return "UP";
        case util::Direction::DOWN: return "DOWN";
        case util::Direction::LEFT: return "LEFT";
        case util::Direction::RIGHT: return "RIGHT";
        default: return "NONE";
    }
}

void HumanView::cycleDirectionClockwise() {
    util::Direction d = m_pendingDirection.value_or(util::Direction::UP);
    switch (d) {
        case util::Direction::UP:    d = util::Direction::RIGHT; break;
        case util::Direction::RIGHT: d = util::Direction::DOWN;  break;
        case util::Direction::DOWN:  d = util::Direction::LEFT;  break;
        case util::Direction::LEFT:  d = util::Direction::UP;    break;
        default: d = util::Direction::UP; break;
    }
    m_pendingDirection = d;
}

void HumanView::handleSDLEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        const int key = event.key.keysym.sym;
        switch (key) {
            case SDLK_UP:
            case SDLK_w:
                m_pendingDirection = util::Direction::UP;
                break;
            case SDLK_DOWN:
            case SDLK_s:
                m_pendingDirection = util::Direction::DOWN;
                break;
            case SDLK_LEFT:
            case SDLK_a:
                m_pendingDirection = util::Direction::LEFT;
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                m_pendingDirection = util::Direction::RIGHT;
                break;
            case SDLK_1:
                m_selectedCardIndex = m_cardScrollOffset + 0;
                updateSelectedCardFromIndex();
                break;
            case SDLK_2:
                m_selectedCardIndex = m_cardScrollOffset + 1;
                updateSelectedCardFromIndex();
                break;
            case SDLK_3:
                m_selectedCardIndex = m_cardScrollOffset + 2;
                updateSelectedCardFromIndex();
                break;
            case SDLK_4:
                m_selectedCardIndex = m_cardScrollOffset + 3;
                updateSelectedCardFromIndex();
                break;
            case SDLK_5:
                m_selectedCardIndex = m_cardScrollOffset + 4;
                updateSelectedCardFromIndex();
                break;
            case SDLK_LEFTBRACKET: {
                if (m_availableCards.empty()) break;
                if (m_selectedCardIndex < 0) m_selectedCardIndex = 0;
                else m_selectedCardIndex = std::max(0, m_selectedCardIndex - 1);
                const int maxOffset = std::max(0, static_cast<int>(m_availableCards.size()) - s_cardSlots);
                m_cardScrollOffset = std::clamp(m_selectedCardIndex - 2, 0, maxOffset);
                updateSelectedCardFromIndex();
                break;
            }
            case SDLK_RIGHTBRACKET: {
                if (m_availableCards.empty()) break;
                if (m_selectedCardIndex < 0) m_selectedCardIndex = 0;
                else m_selectedCardIndex = std::min(static_cast<int>(m_availableCards.size()) - 1, m_selectedCardIndex + 1);
                const int maxOffset = std::max(0, static_cast<int>(m_availableCards.size()) - s_cardSlots);
                m_cardScrollOffset = std::clamp(m_selectedCardIndex - 2, 0, maxOffset);
                updateSelectedCardFromIndex();
                break;
            }
            case SDLK_COMMA:
            case SDLK_LESS:
                m_equipmentScrollOffset = std::max(0, m_equipmentScrollOffset - 1);
                break;
            case SDLK_PERIOD:
            case SDLK_GREATER: {
                const int maxOffset = std::max(0, static_cast<int>(m_equipmentEntries.size()) - s_equipmentVisibleRows);
                m_equipmentScrollOffset = std::min(maxOffset, m_equipmentScrollOffset + 1);
                break;
            }
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (m_selectedCard) m_confirmed = true;
                break;
            case SDLK_q:
                m_weaponCycleRequest = -1;
                break;
            case SDLK_e:
                m_weaponCycleRequest = 1;
                break;
            default:
                break;
        }
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        m_lastMouseX = event.motion.x;
        m_lastMouseY = event.motion.y;
        m_hoveredCardIndex = -1;
        for (int i = 0; i < s_cardSlots; ++i) {
            if (pointInRect(event.motion.x, event.motion.y, getCardRect(i))) {
                m_hoveredCardIndex = i;
                break;
            }
        }
        m_hoveredEquipmentIndex = -1;
        m_hoverTooltip.clear();
        const Rect equipmentPanel = getEquipmentPanelRect();
        if (pointInRect(event.motion.x, event.motion.y, equipmentPanel)) {
            const int relY = event.motion.y - equipmentPanel.y - 8;
            if (relY >= 0) {
                const int row = relY / kEquipmentRowHeight;
                const int entryIndex = m_equipmentScrollOffset + row;
                if (row >= 0 && row < s_equipmentVisibleRows &&
                    entryIndex >= 0 && entryIndex < static_cast<int>(m_equipmentEntries.size())) {
                    m_hoveredEquipmentIndex = entryIndex;
                    m_hoverTooltip = m_equipmentEntries[static_cast<size_t>(entryIndex)].description;
                }
            }
        }
        const SimpleRect backButton = equipmentBackButtonRect();
        const SimpleRect forwardButton = equipmentForwardButtonRect();
        m_hoveringEquipmentScrollBack =
            event.motion.x >= backButton.x && event.motion.x < backButton.x + backButton.w &&
            event.motion.y >= backButton.y && event.motion.y < backButton.y + backButton.h;
        m_hoveringEquipmentScrollForward =
            event.motion.x >= forwardButton.x && event.motion.x < forwardButton.x + forwardButton.w &&
            event.motion.y >= forwardButton.y && event.motion.y < forwardButton.y + forwardButton.h;
        m_hoveringConfirm = pointInRect(event.motion.x, event.motion.y, getConfirmRect());
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        const int mx = event.button.x;
        const int my = event.button.y;

        if (event.button.button == SDL_BUTTON_LEFT) {
            const SimpleRect backButton = equipmentBackButtonRect();
            const SimpleRect forwardButton = equipmentForwardButtonRect();
            if (mx >= backButton.x && mx < backButton.x + backButton.w &&
                my >= backButton.y && my < backButton.y + backButton.h) {
                m_equipmentScrollOffset = std::max(0, m_equipmentScrollOffset - 1);
                return;
            }
            if (mx >= forwardButton.x && mx < forwardButton.x + forwardButton.w &&
                my >= forwardButton.y && my < forwardButton.y + forwardButton.h) {
                const int maxOffset = std::max(0, static_cast<int>(m_equipmentEntries.size()) - s_equipmentVisibleRows);
                m_equipmentScrollOffset = std::min(maxOffset, m_equipmentScrollOffset + 1);
                return;
            }
            for (int i = 0; i < s_cardSlots; ++i) {
                Rect r = getCardRect(i);
                if (pointInRect(mx, my, r)) {
                    m_selectedCardIndex = m_cardScrollOffset + i;
                    updateSelectedCardFromIndex();
                    return;
                }
            }

            if (m_selectedCard && pointInRect(mx, my, getConfirmRect())) {
                m_confirmed = true;
                return;
            }
            if (m_hoveredEquipmentIndex >= 0 &&
                m_hoveredEquipmentIndex < static_cast<int>(m_equipmentEntries.size())) {
                const auto& entry = m_equipmentEntries[static_cast<size_t>(m_hoveredEquipmentIndex)];
                if (entry.isWeapon && !entry.isActiveWeapon && entry.slot >= 0) {
                    m_requestedWeaponSlot = entry.slot;
                    return;
                }
            }
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            cycleDirectionClockwise();
            return;
        }
    }
}

void HumanView::updateSelectedCardFromIndex() {
    if (m_selectedCardIndex >= 0 && m_selectedCardIndex < static_cast<int>(m_availableCards.size()))
        m_selectedCard = m_availableCards[static_cast<size_t>(m_selectedCardIndex)];
    else
        m_selectedCard.reset();
}

void HumanView::setAvailableCards(std::vector<ProgramCard> cards) {
    m_availableCards = std::move(cards);
    const int maxOffset = std::max(0, static_cast<int>(m_availableCards.size()) - s_cardSlots);
    m_cardScrollOffset = std::clamp(m_cardScrollOffset, 0, maxOffset);
    if (m_selectedCardIndex >= static_cast<int>(m_availableCards.size()))
        m_selectedCardIndex = -1;
    updateSelectedCardFromIndex();
}

bool HumanView::hasConfirmed() const {
    return m_confirmed;
}

std::optional<ProgramCard> HumanView::getSelectedCard() const {
    return m_selectedCard;
}

std::optional<util::Direction> HumanView::getPendingDirection() const {
    return m_pendingDirection;
}

void HumanView::setPlayerStats(int hp, int maxHp, int heat, int maxHeat) {
    m_playerHp = hp;
    m_playerMaxHp = std::max(1, maxHp);
    m_playerHeat = heat;
    m_playerMaxHeat = std::max(1, maxHeat);
}

void HumanView::setWeaponSummary(std::string weaponName, std::string weaponDescription) {
    m_weaponName = std::move(weaponName);
    m_weaponDescription = std::move(weaponDescription);
}

void HumanView::setCombatLog(std::vector<std::string> combatLog) {
    m_combatLog = std::move(combatLog);
    m_combatLogText.clear();
    for (size_t i = 0; i < m_combatLog.size(); ++i) {
        m_combatLogText += m_combatLog[i];
        if (i + 1 < m_combatLog.size()) {
            m_combatLogText += '\n';
        }
    }
}

void HumanView::setWorldHoverText(std::string text) {
    m_worldHoverText = std::move(text);
}

void HumanView::setEquipmentEntries(std::vector<EquipmentEntry> entries) {
    m_equipmentEntries = std::move(entries);
    const int maxOffset = std::max(0, static_cast<int>(m_equipmentEntries.size()) - s_equipmentVisibleRows);
    m_equipmentScrollOffset = std::clamp(m_equipmentScrollOffset, 0, maxOffset);
}

std::optional<int> HumanView::consumeRequestedWeaponSlot() {
    const auto out = m_requestedWeaponSlot;
    m_requestedWeaponSlot.reset();
    return out;
}

int HumanView::consumeWeaponCycleRequest() {
    const int out = m_weaponCycleRequest;
    m_weaponCycleRequest = 0;
    return out;
}

void HumanView::clearSelection() {
    m_selectedCard.reset();
    m_pendingDirection.reset();
    m_selectedCardIndex = -1;
    m_hoveredCardIndex = -1;
    m_cardScrollOffset = 0;
    m_hoveredEquipmentIndex = -1;
    m_equipmentScrollOffset = 0;
    m_hoveringEquipmentScrollBack = false;
    m_hoveringEquipmentScrollForward = false;
    m_requestedWeaponSlot.reset();
    m_weaponCycleRequest = 0;
    m_hoverTooltip.clear();
    m_hoveringConfirm = false;
    m_confirmed = false;
}

void HumanView::submitDrawRequests(DrawQueue* queue) {
    if (!queue) return;

    DrawRequest sidebar;
    sidebar.layer = util::DrawLayer::UI_OVERLAY;
    sidebar.usePlaceholderShape = true;
    sidebar.placeholderColor = 0xEE171C28u;
    sidebar.dst = {util::layout::SidebarX, util::layout::SidebarY, util::layout::SidebarW, util::layout::SidebarH};
    sidebar.zOrder = 1800;
    queue->push(sidebar);

    DrawRequest bottomBar;
    bottomBar.layer = util::DrawLayer::UI_OVERLAY;
    bottomBar.usePlaceholderShape = true;
    bottomBar.placeholderColor = 0xEE171C28u;
    bottomBar.dst = {util::layout::BottomBarX, util::layout::BottomBarY, util::layout::BottomBarW, util::layout::BottomBarH};
    bottomBar.zOrder = 1800;
    queue->push(bottomBar);

    const int statBarW = util::layout::SidebarW - kSidebarPad * 2;
    const int statBarH = 20;
    const int hpBarX = util::layout::SidebarX + kSidebarPad;
    const int hpBarY = util::layout::SidebarY + kSidebarPad;

    DrawRequest hpBg;
    hpBg.layer = util::DrawLayer::UI_OVERLAY;
    hpBg.usePlaceholderShape = true;
    hpBg.placeholderColor = 0xFF202020u;
    hpBg.dst = {hpBarX, hpBarY, statBarW, statBarH};
    hpBg.zOrder = 1900;
    queue->push(hpBg);

    int hpW = (m_playerMaxHp > 0) ? (statBarW * m_playerHp / m_playerMaxHp) : 0;
    if (hpW > 0) {
        DrawRequest hpFill;
        hpFill.layer = util::DrawLayer::UI_OVERLAY;
        hpFill.usePlaceholderShape = true;
        hpFill.placeholderColor = 0xFF2AA24Au;
        hpFill.dst = {hpBarX, hpBarY, hpW, statBarH};
        hpFill.zOrder = 1901;
        queue->push(hpFill);
    }

    const int heatBarY = hpBarY + 56;
    DrawRequest heatBg;
    heatBg.layer = util::DrawLayer::UI_OVERLAY;
    heatBg.usePlaceholderShape = true;
    heatBg.placeholderColor = 0xFF202020u;
    heatBg.dst = {hpBarX, heatBarY, statBarW, statBarH};
    heatBg.zOrder = 1900;
    queue->push(heatBg);

    int heatW = (m_playerMaxHeat > 0) ? (statBarW * m_playerHeat / m_playerMaxHeat) : 0;
    if (heatW > 0) {
        DrawRequest heatFill;
        heatFill.layer = util::DrawLayer::UI_OVERLAY;
        heatFill.usePlaceholderShape = true;
        heatFill.placeholderColor = 0xFFC05030u;
        heatFill.dst = {hpBarX, heatBarY, heatW, statBarH};
        heatFill.zOrder = 1901;
        queue->push(heatFill);
    }

    DrawRequest hpText;
    hpText.layer = util::DrawLayer::UI_OVERLAY;
    hpText.hasText = true;
    hpText.textColor = 0xFFFFFFFFu;
    hpText.text = "HP  " + std::to_string(m_playerHp) + " / " + std::to_string(m_playerMaxHp);
    hpText.dst = {hpBarX, hpBarY - 28, statBarW, 24};
    hpText.zOrder = 1910;
    queue->push(hpText);

    DrawRequest heatText;
    heatText.layer = util::DrawLayer::UI_OVERLAY;
    heatText.hasText = true;
    heatText.textColor = 0xFFFFFFFFu;
    heatText.text = "HEAT  " + std::to_string(m_playerHeat) + " / " + std::to_string(m_playerMaxHeat);
    heatText.dst = {hpBarX, heatBarY - 28, statBarW, 24};
    heatText.zOrder = 1910;
    queue->push(heatText);

    DrawRequest facingPanel;
    facingPanel.layer = util::DrawLayer::UI_OVERLAY;
    facingPanel.usePlaceholderShape = true;
    facingPanel.placeholderColor = 0xCC243247u;
    facingPanel.dst = {hpBarX, heatBarY + 56, statBarW, 72};
    facingPanel.zOrder = 1905;
    queue->push(facingPanel);

    DrawRequest facingText;
    facingText.layer = util::DrawLayer::UI_OVERLAY;
    facingText.hasText = true;
    facingText.textColor = 0xFFFFFFFFu;
    facingText.text = std::string("Facing: ") + directionLabel(m_pendingDirection.value_or(util::Direction::RIGHT));
    facingText.dst = {hpBarX + 16, heatBarY + 72, statBarW - 32, 24};
    facingText.zOrder = 1911;
    queue->push(facingText);

//    JUST IN CASE DRAW REQUEST NEEDS TO BE MOVED UP
//    DrawRequest controlsText;
//    controlsText.layer = util::DrawLayer::UI_OVERLAY;
//    controlsText.hasText = true;
//    controlsText.textColor = 0xFFE8EEF8u;
//    controlsText.text = "Rotate with Arrows, WASD, or right mouse. Resize the window freely.";
//    controlsText.dst = {hpBarX, heatBarY + 148, statBarW, 66};
//    controlsText.zOrder = 1911;
//    queue->push(controlsText);

    DrawRequest weaponPanel;
    weaponPanel.layer = util::DrawLayer::UI_OVERLAY;
    weaponPanel.usePlaceholderShape = true;
    weaponPanel.placeholderColor = 0xCC233042u;
    weaponPanel.dst = {hpBarX, util::layout::SidebarY + 236, statBarW, 136};
    weaponPanel.zOrder = 1905;
    queue->push(weaponPanel);

    DrawRequest weaponTitle;
    weaponTitle.layer = util::DrawLayer::UI_OVERLAY;
    weaponTitle.hasText = true;
    weaponTitle.textColor = 0xFFFFE08Au;
    weaponTitle.text = "Equipped Weapon";
    weaponTitle.dst = {hpBarX + 16, util::layout::SidebarY + 248, statBarW - 32, 26};
    weaponTitle.zOrder = 1911;
    queue->push(weaponTitle);

    DrawRequest weaponName;
    weaponName.layer = util::DrawLayer::UI_OVERLAY;
    weaponName.hasText = true;
    weaponName.textColor = 0xFFFFFFFFu;
    weaponName.text = m_weaponName.empty() ? "None" : m_weaponName;
    weaponName.dst = {hpBarX + 16, util::layout::SidebarY + 282, statBarW - 32, 30};
    weaponName.zOrder = 1911;
    queue->push(weaponName);

    DrawRequest weaponDesc;
    weaponDesc.layer = util::DrawLayer::UI_OVERLAY;
    weaponDesc.hasText = true;
    weaponDesc.textColor = 0xFFE8E8E8u;
    weaponDesc.text = m_weaponDescription.empty() ? "No equipped weapon description available." : m_weaponDescription;
    weaponDesc.dst = {hpBarX + 16, util::layout::SidebarY + 318, statBarW - 32, 44};
    weaponDesc.zOrder = 1911;
    queue->push(weaponDesc);

    DrawRequest componentTitle;
    componentTitle.layer = util::DrawLayer::UI_OVERLAY;
    componentTitle.hasText = true;
    componentTitle.textColor = 0xFFFFFFFFu;
    componentTitle.text = "Components";
    componentTitle.dst = {hpBarX, util::layout::SidebarY + 384, 170, 24};
    componentTitle.zOrder = 1911;
    queue->push(componentTitle);

    const bool canScrollEquipmentBack = m_equipmentScrollOffset > 0;
    const bool canScrollEquipmentForward = (m_equipmentScrollOffset + s_equipmentVisibleRows) < static_cast<int>(m_equipmentEntries.size());
    const SimpleRect equipmentBackRect = equipmentBackButtonRect();
    const SimpleRect equipmentForwardRect = equipmentForwardButtonRect();

    DrawRequest equipmentBack;
    equipmentBack.layer = util::DrawLayer::UI_OVERLAY;
    equipmentBack.usePlaceholderShape = true;
    equipmentBack.placeholderColor = canScrollEquipmentBack
        ? (m_hoveringEquipmentScrollBack ? 0xFF5B7FB3u : 0xFF42597Eu)
        : 0xFF2D3646u;
    equipmentBack.dst = {equipmentBackRect.x, equipmentBackRect.y, equipmentBackRect.w, equipmentBackRect.h};
    equipmentBack.zOrder = 1911;
    queue->push(equipmentBack);

    DrawRequest equipmentBackText;
    equipmentBackText.layer = util::DrawLayer::UI_OVERLAY;
    equipmentBackText.hasText = true;
    equipmentBackText.textColor = canScrollEquipmentBack ? 0xFFFFFFFFu : 0xFF9DA8B8u;
    equipmentBackText.text = "<";
    equipmentBackText.dst = {equipmentBackRect.x + 8, equipmentBackRect.y + 4, 10, 16};
    equipmentBackText.zOrder = 1912;
    queue->push(equipmentBackText);

    DrawRequest equipmentForward;
    equipmentForward.layer = util::DrawLayer::UI_OVERLAY;
    equipmentForward.usePlaceholderShape = true;
    equipmentForward.placeholderColor = canScrollEquipmentForward
        ? (m_hoveringEquipmentScrollForward ? 0xFF5B7FB3u : 0xFF42597Eu)
        : 0xFF2D3646u;
    equipmentForward.dst = {equipmentForwardRect.x, equipmentForwardRect.y, equipmentForwardRect.w, equipmentForwardRect.h};
    equipmentForward.zOrder = 1911;
    queue->push(equipmentForward);

    DrawRequest equipmentForwardText;
    equipmentForwardText.layer = util::DrawLayer::UI_OVERLAY;
    equipmentForwardText.hasText = true;
    equipmentForwardText.textColor = canScrollEquipmentForward ? 0xFFFFFFFFu : 0xFF9DA8B8u;
    equipmentForwardText.text = ">";
    equipmentForwardText.dst = {equipmentForwardRect.x + 8, equipmentForwardRect.y + 4, 10, 16};
    equipmentForwardText.zOrder = 1912;
    queue->push(equipmentForwardText);

    DrawRequest componentPanel;
    componentPanel.layer = util::DrawLayer::UI_OVERLAY;
    componentPanel.usePlaceholderShape = true;
    componentPanel.placeholderColor = 0xCC1F2C3Eu;
    componentPanel.dst = {hpBarX, util::layout::SidebarY + 408, statBarW, 124};
    componentPanel.zOrder = 1905;
    queue->push(componentPanel);

    int componentLineY = util::layout::SidebarY + 416;
    const int visibleCount = std::min(s_equipmentVisibleRows, static_cast<int>(m_equipmentEntries.size()) - m_equipmentScrollOffset);
    for (int row = 0; row < visibleCount; ++row) {
        const int entryIndex = m_equipmentScrollOffset + row;
        const auto& entry = m_equipmentEntries[static_cast<size_t>(entryIndex)];
        DrawRequest componentLine;
        componentLine.layer = util::DrawLayer::UI_OVERLAY;
        componentLine.hasText = true;
        componentLine.textColor = (entryIndex == m_hoveredEquipmentIndex) ? 0xFFFFFFFFu : 0xFFE8E8E8u;
        componentLine.textPointSize = 13;
        std::string label = entry.label;
        if (entry.isWeapon && entry.isActiveWeapon) label = "* " + label;
        componentLine.text = label;
        componentLine.dst = {hpBarX + 10, componentLineY, statBarW - 20, 18};
        componentLine.zOrder = 1912;
        queue->push(componentLine);
        componentLineY += kEquipmentRowHeight;
    }

    DrawRequest equipmentHint;
    equipmentHint.layer = util::DrawLayer::UI_OVERLAY;
    equipmentHint.hasText = true;
    equipmentHint.textColor = 0xFFBFD7FFu;
    equipmentHint.textPointSize = 11;
    if (!m_equipmentEntries.empty()) {
        const int shownStart = m_equipmentScrollOffset + 1;
        const int shownEnd = std::min(static_cast<int>(m_equipmentEntries.size()), m_equipmentScrollOffset + s_equipmentVisibleRows);
        equipmentHint.text = "< > browse gear  " + std::to_string(shownStart) + "-" + std::to_string(shownEnd) +
                             " / " + std::to_string(m_equipmentEntries.size());
    } else {
        equipmentHint.text = "< > browse gear";
    }
    equipmentHint.dst = {hpBarX, util::layout::SidebarY + 534, statBarW, 18};
    equipmentHint.zOrder = 1912;
    queue->push(equipmentHint);

    if (!m_hoverTooltip.empty()) {
        DrawRequest tooltipPanel;
        tooltipPanel.layer = util::DrawLayer::UI_OVERLAY;
        tooltipPanel.usePlaceholderShape = true;
        tooltipPanel.placeholderColor = 0xEE111826u;
        tooltipPanel.dst = {hpBarX, util::layout::SidebarY + 556, statBarW, 30};
        tooltipPanel.zOrder = 1913;
        queue->push(tooltipPanel);

        DrawRequest tooltipText;
        tooltipText.layer = util::DrawLayer::UI_OVERLAY;
        tooltipText.hasText = true;
        tooltipText.textColor = 0xFFFFE9A8u;
        tooltipText.textPointSize = 11;
        tooltipText.text = m_hoverTooltip;
        tooltipText.dst = {hpBarX + 8, util::layout::SidebarY + 562, statBarW - 16, 22};
        tooltipText.zOrder = 1914;
        queue->push(tooltipText);
    }

    DrawRequest cardInfoTitle;
    cardInfoTitle.layer = util::DrawLayer::UI_OVERLAY;
    cardInfoTitle.hasText = true;
    cardInfoTitle.textColor = 0xFFFFFFFFu;
    cardInfoTitle.text = m_worldHoverText.empty()
        ? "Combat Log (Q/E swap weapon)"
        : ("Target: " + m_worldHoverText);
    cardInfoTitle.dst = {hpBarX, util::layout::SidebarY + 592, statBarW, 24};
    cardInfoTitle.zOrder = 1911;
    queue->push(cardInfoTitle);

    DrawRequest combatLogPanel;
    combatLogPanel.layer = util::DrawLayer::UI_OVERLAY;
    combatLogPanel.usePlaceholderShape = true;
    combatLogPanel.placeholderColor = 0xCC1D2838u;
    combatLogPanel.dst = {hpBarX, util::layout::SidebarY + 616, statBarW, 112};
    combatLogPanel.zOrder = 1905;
    queue->push(combatLogPanel);

    DrawRequest logLines;
    logLines.layer = util::DrawLayer::UI_OVERLAY;
    logLines.hasText = true;
    logLines.textColor = 0xFFE8E8E8u;
    logLines.textPointSize = 11;
    logLines.text = m_combatLogText;
    logLines.dst = {hpBarX + 10, util::layout::SidebarY + 622, statBarW - 20, 102};
    logLines.zOrder = 1912;
    queue->push(logLines);

    for (int i = 0; i < s_cardSlots; ++i) {
        Rect r = getCardRect(i);
        const int cardIndex = m_cardScrollOffset + i;
        const bool selected = (cardIndex == m_selectedCardIndex);
        const bool hovered = (i == m_hoveredCardIndex);

        DrawRequest card;
        card.layer = util::DrawLayer::UI_OVERLAY;
        card.usePlaceholderShape = true;
        card.placeholderColor = selected ? 0xFF50BAA6u : (hovered ? 0xFF6EEAD7u : 0xFF92F7E3u);
        card.dst = {r.x, r.y, r.w, r.h};
        card.zOrder = 1990 + i;
        queue->push(card);

        if (selected || hovered) {
            DrawRequest glow;
            glow.layer = util::DrawLayer::UI_OVERLAY;
            glow.usePlaceholderShape = true;
            glow.outlineOnly = true;
            glow.placeholderColor = selected ? 0xFFFFCC00u : 0xFFFFFFFFu;
            glow.dst = {r.x - 3, r.y - 3, r.w + 6, r.h + 6};
            glow.zOrder = 1994 + i;
            queue->push(glow);
        }

        if (cardIndex >= 0 && cardIndex < static_cast<int>(m_availableCards.size())) {
            const auto& ccard = m_availableCards[static_cast<size_t>(cardIndex)];
            const int cx = r.x + r.w / 2;
            const int cy = r.y + r.h / 2;

            DrawRequest name;
            name.layer = util::DrawLayer::UI_OVERLAY;
            name.hasText = true;
            name.textColor = 0xFF10233Du;
            name.text = ccard.name;
            name.dst = {r.x + 12, r.y + 8, r.w - 24, 22};
            name.zOrder = 1991 + i;
            queue->push(name);

            DrawRequest rcenter;
            rcenter.layer = util::DrawLayer::UI_OVERLAY;
            rcenter.usePlaceholderShape = true;
            rcenter.placeholderColor = 0xFF0B2038u;
            rcenter.dst = {cx - 3, cy + 12, 6, 6};
            rcenter.zOrder = 1991 + i;
            queue->push(rcenter);

            if (ccard.moveType == util::MoveType::ATTACK) {
                DrawRequest atk;
                atk.layer = util::DrawLayer::UI_OVERLAY;
                atk.usePlaceholderShape = true;
                atk.placeholderColor = hovered ? 0xFFFFB3ACu : 0xFFFF6F61u;
                atk.dst = {cx - 6, cy - 12, 12, 12};
                atk.zOrder = 1992 + i;
                queue->push(atk);
            } else if (ccard.moveType == util::MoveType::MOVE) {
                int px = cx;
                int py = cy + 12;
                for (auto pd : ccard.path) {
                    if (pd == util::RelativeDir::FORWARD) py -= 14;
                    else if (pd == util::RelativeDir::BACKWARD) py += 14;
                    else if (pd == util::RelativeDir::RIGHT) px += 14;
                    else if (pd == util::RelativeDir::LEFT) px -= 14;

                    DrawRequest step;
                    step.layer = util::DrawLayer::UI_OVERLAY;
                    step.usePlaceholderShape = true;
                    step.placeholderColor = hovered ? 0xFFFFFFFFu : 0xFFF4FBFFu;
                    step.dst = {px - 4, py - 4, 8, 8};
                    step.zOrder = 1992 + i;
                    queue->push(step);
                }
            }
        }
    }

    DrawRequest scrollHint;
    scrollHint.layer = util::DrawLayer::UI_OVERLAY;
    scrollHint.hasText = true;
    scrollHint.textColor = 0xFFBFD7FFu;
    scrollHint.textPointSize = 12;
    scrollHint.text = "< / > browse gear | [ / ] change selection";
    scrollHint.dst = {util::layout::BottomBarX + 18, util::layout::BottomBarY + 66, 220, 20};
    scrollHint.zOrder = 2052;
    queue->push(scrollHint);

    DrawRequest pageIndicator;
    pageIndicator.layer = util::DrawLayer::UI_OVERLAY;
    pageIndicator.hasText = true;
    pageIndicator.textColor = 0xFFE8EEF8u;
    pageIndicator.textPointSize = 12;
    if (!m_availableCards.empty()) {
        const int shownStart = m_cardScrollOffset + 1;
        const int shownEnd = std::min(static_cast<int>(m_availableCards.size()), m_cardScrollOffset + s_cardSlots);
        pageIndicator.text = "Cards " + std::to_string(shownStart) + "-" + std::to_string(shownEnd) +
                             " / " + std::to_string(m_availableCards.size());
    } else {
        pageIndicator.text = "Cards 0-0 / 0";
    }
    pageIndicator.dst = {util::layout::BottomBarX + 250, util::layout::BottomBarY + 66, 230, 20};
    pageIndicator.zOrder = 2052;
    queue->push(pageIndicator);

    Rect cr = getConfirmRect();
    DrawRequest confirmBtn;
    confirmBtn.layer = util::DrawLayer::UI_OVERLAY;
    confirmBtn.usePlaceholderShape = true;
    confirmBtn.placeholderColor = m_hoveringConfirm ? 0xFF30B864u : (m_selectedCard ? 0xFF1F9F55u : 0xFF3A3A3Au);
    confirmBtn.dst = {cr.x, cr.y, cr.w, cr.h};
    confirmBtn.zOrder = 2050;
    queue->push(confirmBtn);

    DrawRequest confirmText;
    confirmText.layer = util::DrawLayer::UI_OVERLAY;
    confirmText.hasText = true;
    confirmText.textColor = 0xFFFFFFFFu;
    confirmText.text = "Confirm";
    confirmText.dst = {cr.x + 20, cr.y + 14, cr.w - 40, 24};
    confirmText.zOrder = 2051;
    queue->push(confirmText);
}
