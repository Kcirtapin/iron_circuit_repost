#include "model/PlayerRobot.h"
#include "model/Level.h"
#include "model/Weapon.h"
#include "render/DrawQueue.h"
#include "util/Types.h"
#include "util/GameConfig.h"
#include <SDL.h>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_set>

PlayerRobot::PlayerRobot(int id, util::Vector2i pos, std::weak_ptr<EventBus> eventBus)
    : Robot(id, pos, std::move(eventBus)),
      m_inventory(kMaxInventorySlots) {}

bool PlayerRobot::storeInInventory(std::unique_ptr<Component> component) {
    if (!component) return false;

    for (int i = 0; i < m_inventoryCapacity; ++i) {
        if (!m_inventory[i]) {
            m_inventory[i] = std::move(component);
            return true;
        }
    }

    return false;
}

std::unique_ptr<Component> PlayerRobot::dropFromInventory(int slot) {
    if (slot < 0 || slot >= m_inventoryCapacity) return nullptr;
    return std::move(m_inventory[slot]);
}

void PlayerRobot::updateInventoryCapacityForLevel(int level) {
    if (level < 1) level = 1;

    m_inventoryCapacity = kStartingInventorySlots +
        ((level - 1) * kInventorySlotsPerLevel);

    if (m_inventoryCapacity > kMaxInventorySlots) {
        m_inventoryCapacity = kMaxInventorySlots;
    }
}

std::vector<ProgramCard> PlayerRobot::getAvailablePrograms() const {
    return m_programList;
}

void PlayerRobot::setProgramList(std::vector<ProgramCard> programs) {
    m_programList = std::move(programs);
}

void PlayerRobot::addProgramCard(const ProgramCard& card) {
    m_programList.push_back(card);
}

std::vector<int> PlayerRobot::getWeaponSlots() const {
    std::vector<int> slots;
    for (int i = 0; i < RobotBody::kSlotCount; ++i) {
        if (dynamic_cast<const Weapon*>(m_body.getComponent(i)) != nullptr) {
            slots.push_back(i);
        }
    }
    return slots;
}

const Weapon* PlayerRobot::getActiveWeapon() const {
    const Component* c = m_body.getComponent(m_activeWeaponSlot);
    if (const auto* weapon = dynamic_cast<const Weapon*>(c)) {
        return weapon;
    }
    const auto slots = getWeaponSlots();
    if (slots.empty()) return nullptr;
    const Component* fallback = m_body.getComponent(slots.front());
    return dynamic_cast<const Weapon*>(fallback);
}

bool PlayerRobot::setActiveWeaponSlot(int slot) {
    if (slot < 0 || slot >= RobotBody::kSlotCount) return false;
    if (dynamic_cast<const Weapon*>(m_body.getComponent(slot)) == nullptr) return false;
    m_activeWeaponSlot = slot;
    return true;
}

void PlayerRobot::cycleActiveWeapon(int dir) {
    auto slots = getWeaponSlots();
    if (slots.empty()) return;
    auto it = std::find(slots.begin(), slots.end(), m_activeWeaponSlot);
    int idx = (it == slots.end()) ? 0 : static_cast<int>(std::distance(slots.begin(), it));
    if (dir >= 0) idx = (idx + 1) % static_cast<int>(slots.size());
    else idx = (idx - 1 + static_cast<int>(slots.size())) % static_cast<int>(slots.size());
    m_activeWeaponSlot = slots[static_cast<size_t>(idx)];
}

void PlayerRobot::resetActiveWeaponSlot() {
    const auto slots = getWeaponSlots();
    if (slots.empty()) {
        m_activeWeaponSlot = RobotBody::kWeaponSlot;
    } else {
        m_activeWeaponSlot = slots.front();
    }
}

void PlayerRobot::configurePrototypeItems() {
    m_lifestealPct = 0.0f;
    m_reflectPct = 0.0f;
    m_reflectCooldownTurns = 0;
    m_reflectCooldownRemaining = 0;
    m_timeSlowTurns = 0;
    m_overclockTurns = 0;
    m_overclockSelfDamagePerTurn = 0;
    m_blinkCooldownTurns = 0;
    m_blinkCooldownRemaining = 0;
    m_blinkRange = 0;

    std::ifstream in("resource/config/content_catalog.json");
    if (!in.is_open()) return;

    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return;
    }
    if (!j.contains("items") || !j["items"].is_array()) return;
    std::unordered_set<std::string> selected;
    for (const auto& id : m_selectedItemIds) selected.insert(id);

    for (const auto& item : j["items"]) {
        const std::string id = item.value("id", "");
        if (!selected.empty() && selected.find(id) == selected.end()) continue;
        if (id == "lifesteal") {
            m_lifestealPct = item["effects"].value("heal_pct_on_damage", 0.2f);
        } else if (id == "reactive_armor") {
            m_reflectPct = item["effects"].value("on_hit_reflect_pct", 0.3f);
            const double cd = item["effects"].value("reflect_cooldown", 1.0);
            m_reflectCooldownTurns = std::max(1, static_cast<int>(cd + 0.5));
        } else if (id == "time_slow") {
            const double dur = item.value("duration", 4.0);
            m_timeSlowTurns = std::max(0, static_cast<int>(dur + 0.5));
        } else if (id == "overclock") {
            const double dur = item.value("duration", 5.0);
            m_overclockTurns = std::max(0, static_cast<int>(dur + 0.5));
            m_overclockSelfDamagePerTurn = item["effects"].value("self_damage_per_sec", 1);
        } else if (id == "blink") {
            const double cd = item.value("cooldown", 6.0);
            m_blinkCooldownTurns = std::max(1, static_cast<int>(cd + 0.5));
            m_blinkRange = item["params"].value("range", 10);
        }
    }
}

void PlayerRobot::onTurnStartItemTick() {
    if (m_reflectCooldownRemaining > 0) --m_reflectCooldownRemaining;
    if (m_blinkCooldownRemaining > 0) --m_blinkCooldownRemaining;
    if (m_timeSlowTurns > 0) --m_timeSlowTurns;
    if (m_overclockTurns > 0) {
        --m_overclockTurns;
        if (m_overclockSelfDamagePerTurn > 0) {
            // Self-damage intentionally bypasses defense as an overclock tradeoff.
            m_hp = std::max(0, m_hp - m_overclockSelfDamagePerTurn);
        }
    }
    if (m_disabledTurns > 0) --m_disabledTurns;

    // Reset component HP at start of turn
    for (int slot = 0; slot < RobotBody::kSlotCount; ++slot) {
        Component* comp = m_body.getComponent(slot);
        if (comp) {
            comp->resetRoundStatus();
        }
    }
}

int PlayerRobot::applyReactiveArmor(int incomingDamage) {
    if (incomingDamage <= 0 || m_reflectPct <= 0.0f || m_reflectCooldownRemaining > 0) return 0;
    m_reflectCooldownRemaining = m_reflectCooldownTurns;
    return std::max(1, static_cast<int>(incomingDamage * m_reflectPct));
}

bool PlayerRobot::tryBlinkMove(Level* level, util::Direction dir) {
    if (!level || m_blinkRange <= 0 || m_blinkCooldownRemaining > 0) return false;
    const util::Vector2i d = util::directionOffset(dir);
    for (int i = 1; i <= m_blinkRange; ++i) {
        util::Vector2i p{m_pos.x + d.x * i, m_pos.y + d.y * i};
        if (!level->isInBounds(p)) break;
        if (level->getTileRef(p.x, p.y).isWalkable()) {
            level->moveRobot(this, p);
            m_blinkCooldownRemaining = m_blinkCooldownTurns;
            return true;
        }
    }
    return false;
}

void PlayerRobot::setRecentActionLabel(std::string text, int turnsToShow) {
    m_recentActionLabel = std::move(text);
    m_recentActionTurns = std::max(0, turnsToShow);
}

void PlayerRobot::clearRecentActionLabel() {
    m_recentActionLabel.clear();
    m_recentActionTurns = 0;
}

void PlayerRobot::decayRecentActionLabel() {
    if (m_recentActionTurns > 0) {
        --m_recentActionTurns;
        if (m_recentActionTurns == 0) {
            m_recentActionLabel.clear();
        }
    }
}

void PlayerRobot::takeDamage(int d) {
    // Heat amplification: higher heat means more damage taken
    int amplifiedDamage = d;
    if (m_heat >= 3) {
        amplifiedDamage = static_cast<int>(std::ceil(static_cast<float>(d) * 1.5f));
    }
    Robot::takeDamage(amplifiedDamage);
}

void PlayerRobot::submitDrawRequests(DrawQueue* queue) {
    submitDrawRequests(queue, 0, 0);
}

void PlayerRobot::submitDrawRequests(DrawQueue* queue, int pixelOffsetX, int pixelOffsetY) {
    if (!queue) return;
    const int cell = util::layout::CellSize;
    const int pad = util::layout::RobotPad;
    const int barH = util::layout::HealthBarHeight;
    const int px = m_pos.x * cell + pixelOffsetX;
    const int py = m_pos.y * cell + pixelOffsetY;

    DrawRequest body;
    body.layer = util::DrawLayer::ROBOTS;
    body.usePlaceholderShape = true;
    body.placeholderColor = 0xFF1155DD;
    body.texturePath = "resource/image/robot_sprite_player.png";
    body.dst.x = px + pad;
    body.dst.y = py + pad + barH + 6;
    body.dst.w = cell - pad * 2;
    body.dst.h = cell - pad * 2 - barH - 6;
    body.zOrder = 1000;
    queue->push(body);

    DrawRequest facing;
    facing.layer = util::DrawLayer::ROBOTS;
    facing.usePlaceholderShape = true;
    facing.placeholderColor = 0xFFAADDFF;
    facing.dst.w = facing.dst.h = 12;
    facing.zOrder = 1001;
    switch (m_facing) {
        case util::Direction::UP:
            facing.dst.x = px + cell / 2 - 6;
            facing.dst.y = py + pad + barH + 6;
            break;
        case util::Direction::DOWN:
            facing.dst.x = px + cell / 2 - 6;
            facing.dst.y = py + cell - pad - 12;
            break;
        case util::Direction::LEFT:
            facing.dst.x = px + pad;
            facing.dst.y = py + cell / 2 - 6;
            break;
        default:
            facing.dst.x = px + cell - pad - 12;
            facing.dst.y = py + cell / 2 - 6;
            break;
    }
    queue->push(facing);

    const int barW = cell - pad * 2;
    DrawRequest hpBg;
    hpBg.layer = util::DrawLayer::UI_WORLD;
    hpBg.usePlaceholderShape = true;
    hpBg.placeholderColor = 0xFF3A0000;
    hpBg.dst = { px + pad, py + pad, barW, barH };
    hpBg.zOrder = 1100;
    queue->push(hpBg);

    int hpFill = (m_maxHp > 0) ? (barW * m_hp / m_maxHp) : 0;
    if (hpFill > 0) {
        int pct = m_hp * 100 / m_maxHp;
        unsigned fillColor = (pct > 60) ? 0xFF22CC44
                           : (pct > 30) ? 0xFFDDAA00
                           :               0xFFCC2200;
        DrawRequest hpFg;
        hpFg.layer = util::DrawLayer::UI_WORLD;
        hpFg.usePlaceholderShape = true;
        hpFg.placeholderColor = fillColor;
        hpFg.dst = { px + pad, py + pad, hpFill, barH };
        hpFg.zOrder = 1101;
        queue->push(hpFg);
    }

    DrawRequest status;
    status.layer = util::DrawLayer::UI_WORLD;
    status.hasText = true;
    status.textPointSize = 11;
    status.textColor = 0xFFB6F7FFu;
    std::string label = "P";
    if (isOverclockActive()) label += " OC";
    if (isTimeSlowActive()) label += " SLOW";
    if (isDisabled()) label += " DIS";
    if (m_blinkCooldownRemaining > 0) label += " BLK:" + std::to_string(m_blinkCooldownRemaining);
    status.text = label;
    status.dst = {px + pad, py + cell - 14, barW, 12};
    status.zOrder = 1102;
    queue->push(status);

    if (!m_recentActionLabel.empty() && m_recentActionTurns > 0) {
        const float wave = std::sin(static_cast<float>(SDL_GetTicks()) * 0.03f);
        const int jiggleX = static_cast<int>(std::round(wave * 5.0f));
        DrawRequest actionTextShadow;
        actionTextShadow.layer = util::DrawLayer::UI_WORLD;
        actionTextShadow.hasText = true;
        actionTextShadow.textColor = 0xCC000000u;
        actionTextShadow.textPointSize = 12;
        actionTextShadow.text = m_recentActionLabel;
        actionTextShadow.dst = {px + pad + 1 + jiggleX, py - 41, cell - pad * 2, 18};
        actionTextShadow.zOrder = 1103;
        queue->push(actionTextShadow);

        DrawRequest actionText;
        actionText.layer = util::DrawLayer::UI_WORLD;
        actionText.hasText = true;
        actionText.textColor = 0xFFFFD84Du;
        actionText.textPointSize = 12;
        actionText.text = m_recentActionLabel;
        actionText.dst = {px + pad + jiggleX, py - 42, cell - pad * 2, 18};
        actionText.zOrder = 1104;
        queue->push(actionText);
    }
}