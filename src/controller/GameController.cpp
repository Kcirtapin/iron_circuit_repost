#include "controller/GameController.h"
#include "audio/AudioManager.h"
#include "controller/HumanController.h"
#include "model/GameModel.h"
#include "model/PlayerRobot.h"
#include "model/Robot.h"
#include "model/Level.h"
#include "view/HumanView.h"
#include "view/UIView.h"
#include "render/Renderer.h"
#include "render/DrawQueue.h"
#include "events/EventBus.h"
#include "util/Types.h"
#include "util/GameConfig.h"
#include "model/Weapon.h"
#include "model/Component.h"
#include "model/RobotBody.h"
#include "model/EnemyRobot.h"
#include <SDL.h>
#include <algorithm>
#include <cassert>
#include <memory>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr unsigned kPostResolveLabelMs = 550;

struct PreviewPathResult {
    std::vector<util::Vector2i> tiles;
    util::Vector2i finalPos;
};

const Weapon* findEquippedAttackWeapon(PlayerRobot* player) {
    return player ? player->getActiveWeapon() : nullptr;
}

ProgramCard makeMoveCard(std::string name, std::string description, std::vector<util::RelativeDir> path) {
    ProgramCard card;
    card.name = std::move(name);
    card.description = std::move(description);
    card.moveType = util::MoveType::MOVE;
    card.range = static_cast<int>(path.size());
    card.path = std::move(path);
    return card;
}

ProgramCard makeAttackCard(std::string name, std::string description) {
    ProgramCard card;
    card.name = std::move(name);
    card.description = std::move(description);
    card.moveType = util::MoveType::ATTACK;
    card.range = 1;
    return card;
}

std::vector<ProgramCard> defaultRunMoveCards() {
    return {
        makeAttackCard("Attack", "No movement; attack from current tile."),
        makeMoveCard("Dash", "Advance two tiles.", {util::RelativeDir::FORWARD, util::RelativeDir::FORWARD}),
        makeMoveCard("Move and Turn Left", "Advance two tiles, then step left.", {util::RelativeDir::FORWARD, util::RelativeDir::FORWARD, util::RelativeDir::LEFT}),
        makeMoveCard("Move and Turn Right", "Advance two tiles, then step right.", {util::RelativeDir::FORWARD, util::RelativeDir::FORWARD, util::RelativeDir::RIGHT}),
    };
}

ProgramCard generatedMoveCardForLevel(int levelIndex) {
    const int pattern = levelIndex % 6;
    switch (pattern) {
        case 0:
            return makeMoveCard("Long Dash", "Advance three tiles.", {util::RelativeDir::FORWARD, util::RelativeDir::FORWARD, util::RelativeDir::FORWARD});
        case 1:
            return makeMoveCard("Cut Left", "Advance then cut left.", {util::RelativeDir::FORWARD, util::RelativeDir::LEFT});
        case 2:
            return makeMoveCard("Cut Right", "Advance then cut right.", {util::RelativeDir::FORWARD, util::RelativeDir::RIGHT});
        case 3:
            return makeMoveCard("Retreat Step", "Step backward one tile.", {util::RelativeDir::BACKWARD});
        case 4:
            return makeMoveCard("Hook Left", "Advance, left, advance.", {util::RelativeDir::FORWARD, util::RelativeDir::LEFT, util::RelativeDir::FORWARD});
        case 5:
        default:
            return makeMoveCard("Hook Right", "Advance, right, advance.", {util::RelativeDir::FORWARD, util::RelativeDir::RIGHT, util::RelativeDir::FORWARD});
    }
}

ProgramCard defaultMoveCardForId(int id) {
    switch (id) {
        case 1:
            return makeMoveCard("Sidestep", "Advance and shift right.", {util::RelativeDir::FORWARD, util::RelativeDir::RIGHT});
        case 2:
            return makeMoveCard("Surge", "Advance, shift right, then advance.", {util::RelativeDir::FORWARD, util::RelativeDir::RIGHT, util::RelativeDir::FORWARD});
        case 3:
            return makeMoveCard("Retreat", "Step backward to disengage.", {util::RelativeDir::BACKWARD});
        case 4:
            return makeMoveCard("Brace Step", "Advance and shift left.", {util::RelativeDir::FORWARD, util::RelativeDir::LEFT});
        case 100:
            return makeMoveCard("Corner Cut", "Shift left then advance.", {util::RelativeDir::LEFT, util::RelativeDir::FORWARD});
        case 101:
            return makeMoveCard("Overrun", "Advance three tiles.", {util::RelativeDir::FORWARD, util::RelativeDir::FORWARD, util::RelativeDir::FORWARD});
        case 102:
            return makeMoveCard("Vector Shift", "Shift right then advance.", {util::RelativeDir::RIGHT, util::RelativeDir::FORWARD});
        case 103:
            return makeMoveCard("Backstep Coil", "Retreat two tiles.", {util::RelativeDir::BACKWARD, util::RelativeDir::BACKWARD});
        case 104:
            return makeMoveCard("Hook Step", "Advance, hook left, advance.", {util::RelativeDir::FORWARD, util::RelativeDir::LEFT, util::RelativeDir::FORWARD});
        case 0:
        default:
            return makeMoveCard("Dash", "Advance two tiles.", {util::RelativeDir::FORWARD, util::RelativeDir::FORWARD});
    }
}

int weaponSelectionRound(int choice, const std::unordered_map<int, int>& weaponRounds) {
    const auto it = weaponRounds.find(choice);
    return (it != weaponRounds.end() && it->second > 0) ? it->second : 1;
}

std::unique_ptr<Component> makePrototypeWeapon(int choice, int selectionRound = 1) {
    ProgramCard card;
    const auto scaledDamage = [&](int base) {
        return std::max(1, base + (selectionRound - 1));
    };
    switch (choice) {
        case 1:
            card.name = "Laser Shot";
            card.description = "Fire a beam straight ahead until the first wall or target.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 4;
            return std::make_unique<Weapon>(
                "Laser",
                "Piercing line shot that punishes enemies from range.",
                util::WeaponType::RANGED,
                scaledDamage(2),
                4,
                std::move(card),
                Weapon::AttackPattern::RAY_UNTIL_WALL);
        case 2:
            card.name = "Arc Swing";
            card.description = "Sweep the tiles in a short arc in front of you.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 1;
            return std::make_unique<Weapon>(
                "Arc Blade",
                "Sweeping melee strike that covers a three-tile fan.",
                util::WeaponType::MELEE,
                scaledDamage(2),
                1,
                std::move(card),
                Weapon::AttackPattern::ARC_90_1);
        case 3:
            card.name = "Charge Beam";
            card.description = "High-voltage line blast.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 6;
            return std::make_unique<Weapon>(
                "Charge Beam",
                "Heavy long-line shot with high damage.",
                util::WeaponType::RANGED,
                scaledDamage(12),
                6,
                std::move(card),
                Weapon::AttackPattern::RAY_UNTIL_WALL);
        case 4:
            card.name = "Chain Lightning";
            card.description = "Arcing multi-target pulse.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 5;
            return std::make_unique<Weapon>(
                "Chain Lightning",
                "Arcing burst modeled as short fan pulse.",
                util::WeaponType::RANGED,
                scaledDamage(8),
                2,
                std::move(card),
                Weapon::AttackPattern::ARC_90_1);
        case 5:
            card.name = "Shotgun";
            card.description = "Wide close-range spread.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 2;
            return std::make_unique<Weapon>(
                "Shotgun",
                "High-impact spread blast.",
                util::WeaponType::RANGED,
                scaledDamage(6),
                2,
                std::move(card),
                Weapon::AttackPattern::ARC_90_1);
        case 6:
            card.name = "Railgun";
            card.description = "Piercing beam through targets.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 12;
            return std::make_unique<Weapon>(
                "Railgun",
                "Long piercing line shot.",
                util::WeaponType::RANGED,
                scaledDamage(20),
                12,
                std::move(card),
                Weapon::AttackPattern::LINE_FIXED);
        case 7:
            card.name = "Pulse Pike";
            card.description = "Extended melee stab.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 2;
            return std::make_unique<Weapon>(
                "Pulse Pike",
                "Two-tile piercing thrust.",
                util::WeaponType::MELEE,
                scaledDamage(9),
                2,
                std::move(card),
                Weapon::AttackPattern::LINE_FIXED);
        case 8:
            card.name = "Ion Carbine";
            card.description = "Stable medium-range rifle.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 5;
            return std::make_unique<Weapon>(
                "Ion Carbine",
                "Reliable medium-range line fire.",
                util::WeaponType::RANGED,
                scaledDamage(7),
                5,
                std::move(card),
                Weapon::AttackPattern::RAY_UNTIL_WALL);
        case 9:
            card.name = "Thermal Lance";
            card.description = "Focused burning strike.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 3;
            return std::make_unique<Weapon>(
                "Thermal Lance",
                "Focused close-line burn.",
                util::WeaponType::MELEE,
                scaledDamage(11),
                3,
                std::move(card),
                Weapon::AttackPattern::LINE_FIXED);
        case 10:
            card.name = "Arc Repeater";
            card.description = "Fast arc suppression.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 2;
            return std::make_unique<Weapon>(
                "Arc Repeater",
                "Rapid arc-pattern pressure.",
                util::WeaponType::RANGED,
                scaledDamage(5),
                2,
                std::move(card),
                Weapon::AttackPattern::ARC_90_1);
        case 11:
            card.name = "Volt Saber";
            card.description = "Heavy front slash.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 1;
            return std::make_unique<Weapon>(
                "Volt Saber",
                "High-damage front melee strike.",
                util::WeaponType::MELEE,
                scaledDamage(10),
                1,
                std::move(card),
                Weapon::AttackPattern::FRONT_TILE);
        case 12:
            card.name = "Turret Deploy";
            card.description = "Deploys a nearby turret pulse.";
            card.moveType = util::MoveType::SPECIAL;
            card.range = 1;
            return std::make_unique<Weapon>(
                "Turret Deploy",
                "Deploys a temporary pulse turret around you.",
                util::WeaponType::RANGED,
                scaledDamage(3),
                1,
                std::move(card),
                Weapon::AttackPattern::FRONT_TILE);
        case 0:
        default:
            card.name = "Sword";
            card.description = "Slash the tile directly in front of you.";
            card.moveType = util::MoveType::ATTACK;
            card.range = 1;
            return std::make_unique<Weapon>(
                "Sword",
                "Reliable melee strike for precise front-tile hits.",
                util::WeaponType::MELEE,
                scaledDamage(3),
                1,
                std::move(card),
                Weapon::AttackPattern::FRONT_TILE);
    }
}

std::string weaponNameFromChoice(int choice) {
    switch (choice) {
        case 1: return "Laser";
        case 2: return "Arc Blade";
        case 3: return "Charge Beam";
        case 4: return "Chain Lightning";
        case 5: return "Shotgun";
        case 6: return "Railgun";
        case 7: return "Pulse Pike";
        case 8: return "Ion Carbine";
        case 9: return "Thermal Lance";
        case 10: return "Arc Repeater";
        case 11: return "Volt Saber";
        case 12: return "Turret Deploy";
        case 0:
        default: return "Sword";
    }
}

std::string weaponRewardDetail(int choice) {
    switch (choice) {
        case 1: return "Store a ranged line weapon for a future loadout revision.";
        case 2: return "Store a sweeping melee arc weapon for a future loadout revision.";
        case 3: return "Store a high-voltage charge beam.";
        case 4: return "Store a chain-lightning style arc weapon.";
        case 5: return "Store a close-range spread shotgun.";
        case 6: return "Store a heavy piercing railgun.";
        case 7: return "Store a two-tile melee pike.";
        case 8: return "Store a reliable ion carbine.";
        case 9: return "Store a thermal line lance.";
        case 10: return "Store a fast arc repeater.";
        case 11: return "Store a high-impact volt saber.";
        case 12: return "Store a deployable turret emitter.";
        case 0:
        default: return "Store a reliable front-tile sword for a future loadout revision.";
    }
}

std::unique_ptr<Component> makePrototypeSupport(int id) {
    switch (id) {
        case 1:
            return std::make_unique<ModuleComponent>(
                "Armor Plating",
                "Heavy armor weave. Grants Sidestep and +1 defense.",
                defaultMoveCardForId(1),
                0, 0, 1, 0);
        case 2:
            return std::make_unique<ModuleComponent>(
                "Power Redirector",
                "Routes spare current into the frame. Grants Surge and +1 attack.",
                defaultMoveCardForId(2),
                0, 1, 0, 0);
        case 3:
            return std::make_unique<ModuleComponent>(
                "Coolant Loop",
                "Bleeds reactor heat. Grants Retreat and +2 max heat.",
                defaultMoveCardForId(3),
                0, 0, 0, 2);
        case 4:
            return std::make_unique<ModuleComponent>(
                "Hull Patch Kit",
                "Emergency chassis braces. Grants Brace Step and +2 max HP.",
                defaultMoveCardForId(4),
                2, 0, 0, 0);
        case 0:
        default:
            return std::make_unique<ModuleComponent>(
                "Engine Jailhack",
                "Overclocked leg actuators. Grants Dash and +1 max heat.",
                defaultMoveCardForId(0),
                0, 0, 0, 1);
    }
}

std::unique_ptr<Component> makeConfiguredSupport(int supportId, int moveId) {
    auto component = makePrototypeSupport(supportId);
    if (auto* module = dynamic_cast<ModuleComponent*>(component.get())) {
        module->setProgramCard(defaultMoveCardForId(moveId));
    }
    return component;
}

std::string supportNameFromId(int id) {
    if (id >= 100) {
        return defaultMoveCardForId(id).name;
    }
    auto component = makePrototypeSupport(id);
    if (const auto* module = dynamic_cast<const ModuleComponent*>(component.get())) {
        return module->getName();
    }
    return "Unknown";
}

std::string supportDetailFromId(int id) {
    if (id >= 100) {
        return defaultMoveCardForId(id).description;
    }
    auto component = makePrototypeSupport(id);
    if (const auto* module = dynamic_cast<const ModuleComponent*>(component.get())) {
        return module->getDescription();
    }
    return "Unknown support module.";
}

int durabilityBonusForLevel(int levelIndex) {
    if (levelIndex <= 0) return 0;
    return 6 + (std::max(1, levelIndex) - 1) * 2;
}

void resetPlayerToBaseStats(PlayerRobot* player, int levelIndex) {
    if (!player) return;
    util::PlayerConfig cfg = util::loadPlayerConfig();
    const int maxHp = (cfg.valid ? cfg.maxHp : 12) + durabilityBonusForLevel(levelIndex);
    const int attackPower = cfg.valid ? cfg.attackPower : 2;
    const int defense = cfg.valid ? cfg.defense : 0;
    const int maxHeat = cfg.valid ? cfg.maxHeat : 10;
    player->setMaxHp(maxHp);
    player->setHp(maxHp);
    player->setAttackPower(attackPower);
    player->setDefense(defense);
    player->setMaxHeat(maxHeat);
}

void configurePrototypeLoadout(PlayerRobot* player, int weaponChoice, const std::vector<int>& ownedWeaponChoices,
                               const std::vector<int>& supportIds, const std::vector<int>& moveIds,
                               int levelIndex, const std::unordered_map<int, int>& weaponSelectionRounds) {
    if (!player) return;
    (void)ownedWeaponChoices;

    resetPlayerToBaseStats(player, levelIndex);
    for (int slot = 0; slot < RobotBody::kSlotCount; ++slot) {
        player->getBody()->clearComponent(slot);
    }

    // Only the currently selected weapon is equipped into the chassis.
    player->getBody()->equipComponent(makePrototypeWeapon(weaponChoice, weaponSelectionRound(weaponChoice, weaponSelectionRounds)), 0);
    const int slotCursor = 1;

    const int moduleCount = std::min(static_cast<int>(supportIds.size()), RobotBody::kSlotCount - slotCursor);
    for (int i = 0; i < moduleCount; ++i) {
        const int supportId = supportIds[static_cast<size_t>(i)];
        const int moveId = (i < static_cast<int>(moveIds.size())) ? moveIds[static_cast<size_t>(i)] : supportId;
        auto component = makeConfiguredSupport(supportId, moveId);
        const auto* module = dynamic_cast<const ModuleComponent*>(component.get());
        if (module) {
            player->setMaxHp(player->getMaxHp() + module->getBonusMaxHp());
            player->setAttackPower(player->getAttackPower() + module->getBonusAttack());
            player->setDefense(player->getDefense() + module->getBonusDefense());
            player->setMaxHeat(player->getMaxHeat() + module->getBonusMaxHeat());
        }
        player->getBody()->equipComponent(std::move(component), slotCursor + i);
    }

    // Set component HP after equipping
    for (int slot = 0; slot < RobotBody::kSlotCount; ++slot) {
        Component* comp = player->getBody()->getComponent(slot);
        if (comp && comp->hasDurability()) {
            const int componentHp = std::max(1, player->getMaxHp() / 4);
            comp->setMaxHp(componentHp);
            comp->setCurrentHp(componentHp);
        }
    }

    player->resetActiveWeaponSlot();
    player->setHp(player->getMaxHp());
}

PreviewPathResult computePreviewPath(Level* level, util::Vector2i startPos,
                                     util::Direction facing, const ProgramCard& card) {
    PreviewPathResult result;
    result.finalPos = startPos;
    if (!level) return result;

    util::Vector2i current = startPos;
    for (const auto& relDir : card.path) {
        const util::Direction worldDir = util::resolveRelative(facing, relDir);
        const util::Vector2i offset = util::directionOffset(worldDir);
        const util::Vector2i next{current.x + offset.x, current.y + offset.y};
        if (!level->isInBounds(next) || !level->getTileRef(next.x, next.y).isWalkable()) {
            break;
        }
        result.tiles.push_back(next);
        current = next;
    }

    result.finalPos = current;
    return result;
}

void pushPreviewTile(DrawQueue* queue, util::Vector2i pos, unsigned color, int zOrder) {
    if (!queue) return;
    const int cell = util::layout::CellSize;
    DrawRequest tile;
    tile.layer = util::DrawLayer::EFFECTS;
    tile.usePlaceholderShape = true;
    tile.placeholderColor = color;
    tile.dst.x = pos.x * cell + 18;
    tile.dst.y = pos.y * cell + 18;
    tile.dst.w = cell - 36;
    tile.dst.h = cell - 36;
    tile.zOrder = zOrder;
    queue->push(tile);
}

void pushEnemyAttackTelegraph(DrawQueue* queue, Level* level, util::Vector2i from, util::Vector2i target, int zBase) {
    if (!queue || !level) return;
    if (from.x == target.x) {
        const int step = (target.y >= from.y) ? 1 : -1;
        for (int y = from.y + step; ; y += step) {
            const util::Vector2i pos{from.x, y};
            if (!level->isInBounds(pos)) break;
            pushPreviewTile(queue, pos, 0x88FF4D4Du, zBase++);
            if (y == target.y) break;
        }
        return;
    }
    if (from.y == target.y) {
        const int step = (target.x >= from.x) ? 1 : -1;
        for (int x = from.x + step; ; x += step) {
            const util::Vector2i pos{x, from.y};
            if (!level->isInBounds(pos)) break;
            pushPreviewTile(queue, pos, 0x88FF4D4Du, zBase++);
            if (x == target.x) break;
        }
        return;
    }
    if (level->isInBounds(target)) {
        pushPreviewTile(queue, target, 0x88FF4D4Du, zBase);
    }
}

void pushFacingPreview(DrawQueue* queue, util::Vector2i pos, util::Direction facing) {
    if (!queue) return;
    const int cell = util::layout::CellSize;
    const int pad = util::layout::RobotPad;

    DrawRequest outline;
    outline.layer = util::DrawLayer::EFFECTS;
    outline.usePlaceholderShape = true;
    outline.outlineOnly = true;
    outline.placeholderColor = 0xFFFFD84Du;
    outline.dst.x = pos.x * cell + pad - 3;
    outline.dst.y = pos.y * cell + pad + util::layout::HealthBarHeight + 3;
    outline.dst.w = cell - pad * 2 + 6;
    outline.dst.h = cell - pad * 2 - util::layout::HealthBarHeight;
    outline.zOrder = 9050;
    queue->push(outline);

    DrawRequest arrow;
    arrow.layer = util::DrawLayer::EFFECTS;
    arrow.usePlaceholderShape = true;
    arrow.placeholderColor = 0xFFFFF0AAu;
    arrow.dst.w = 14;
    arrow.dst.h = 14;
    arrow.zOrder = 9051;
    switch (facing) {
        case util::Direction::UP:
            arrow.dst.x = pos.x * cell + cell / 2 - 7;
            arrow.dst.y = pos.y * cell + pad + util::layout::HealthBarHeight + 4;
            break;
        case util::Direction::DOWN:
            arrow.dst.x = pos.x * cell + cell / 2 - 7;
            arrow.dst.y = pos.y * cell + cell - pad - 14;
            break;
        case util::Direction::LEFT:
            arrow.dst.x = pos.x * cell + pad + 4;
            arrow.dst.y = pos.y * cell + cell / 2 - 7;
            break;
        case util::Direction::RIGHT:
        default:
            arrow.dst.x = pos.x * cell + cell - pad - 18;
            arrow.dst.y = pos.y * cell + cell / 2 - 7;
            break;
    }
    queue->push(arrow);
}

std::pair<std::string, std::string> getWeaponSummary(const Weapon* weapon) {
    if (!weapon) {
        return {"No weapon equipped", "Equip a weapon to attack after movement."};
    }

    const std::string type = (weapon->getWeaponType() == util::WeaponType::RANGED) ? "RANGED" : "MELEE";
    std::string description = weapon->getDescription();
    if (!description.empty()) description += " | ";
    description += type + " | DMG " + std::to_string(weapon->getDamage()) + " | RNG " + std::to_string(weapon->getRange());
    return {weapon->getName(), description};
}

std::vector<HumanView::EquipmentEntry> getEquipmentEntries(PlayerRobot* player,
                                                           const std::vector<int>& ownedWeaponChoices,
                                                           const std::vector<int>& ownedSupportIds,
                                                           int equippedWeaponChoice,
                                                           const std::unordered_map<int, int>& selectionRounds) {
    std::vector<HumanView::EquipmentEntry> lines;
    if (!player) return lines;

    // Show equipped components with HP info
    for (int slot = 0; slot < RobotBody::kSlotCount; ++slot) {
        const Component* comp = player->getBody()->getComponent(slot);
        if (!comp) continue;
        HumanView::EquipmentEntry entry;
        entry.slot = slot;
        entry.isWeapon = dynamic_cast<const Weapon*>(comp) != nullptr;
        entry.isActiveWeapon = (slot == player->getActiveWeaponSlot());
        entry.label = comp->getName();
        if (comp->hasDurability()) {
            entry.label += " (" + std::to_string(comp->getCurrentHp()) + "/" + std::to_string(comp->getMaxHp()) + " HP)";
            if (comp->isDisabled()) {
                entry.label += " [DISABLED]";
            }
        }
        entry.description = comp->getDescription().empty() ? "No description." : comp->getDescription();
        if (comp->hasDurability()) {
            entry.description += "\nHP: " + std::to_string(comp->getCurrentHp()) + "/" + std::to_string(comp->getMaxHp());
            if (comp->isDisabled()) {
                entry.description += " (Disabled until next turn)";
            }
        }
        lines.push_back(std::move(entry));
    }

    if (lines.empty()) {
        HumanView::EquipmentEntry entry;
        entry.label = "No equipped gear";
        entry.description = "Equip components to see them here.";
        lines.push_back(std::move(entry));
    }
    return lines;
}

util::Direction directionToward(util::Vector2i from, util::Vector2i to) {
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    if (std::abs(dx) >= std::abs(dy)) {
        return dx >= 0 ? util::Direction::RIGHT : util::Direction::LEFT;
    }
    return dy >= 0 ? util::Direction::DOWN : util::Direction::UP;
}

} // namespace

GameController::GameController() = default;
GameController::~GameController() { shutdownSDL(); }

void GameController::rebuildHumanController() {
    m_humanController = std::make_unique<HumanController>(
        m_model->getPlayer(), m_model.get(), m_humanView.get());
}

void GameController::loadRunLevel(int levelIndex) {
    m_runLevel = std::max(0, levelIndex);
    m_defeatScreenOpen = false;
    m_model->loadLevel(m_runLevel);
    rebuildHumanController();
    applyCurrentLoadout();
    m_humanController->syncCardsToView();
    m_humanView->clearSelection();
}

void GameController::applyCurrentLoadout() {
    configurePrototypeLoadout(m_model->getPlayer(), m_equippedWeaponChoice, m_ownedWeaponChoices,
                              m_equippedSupportIds, m_equippedMoveIds, m_runLevel, m_weaponSelectionRounds);
    if (auto* player = m_model->getPlayer()) {
        player->setSelectedItemIds(m_selectedItemIds);
        player->configurePrototypeItems();
        player->setProgramList(m_runMoveCards);
    }
}

void GameController::openRewardScreen(bool isInitialLoadout) {
    m_initialRewardSelectionOpen = isInitialLoadout;
    const std::vector<int> allWeaponChoices{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::vector<int> unowned;
    for (int w : allWeaponChoices) {
        if (std::find(m_ownedWeaponChoices.begin(), m_ownedWeaponChoices.end(), w) == m_ownedWeaponChoices.end()) {
            unowned.push_back(w);
        }
    }
    if (unowned.empty()) {
        unowned = allWeaponChoices;
    }
    m_pendingRewardWeaponChoices.clear();
    for (int i = 0; i < 3; ++i) {
        const int idx = (m_runLevel + i) % static_cast<int>(unowned.size());
        m_pendingRewardWeaponChoices.push_back(unowned[static_cast<size_t>(idx)]);
    }
    m_pendingRewardComponentChoices = {
        (m_runLevel + 0) % 5,
        (m_runLevel + 1) % 5,
        (m_runLevel + 2) % 5
    };
    m_pendingRewardMoveChoices = {
        generatedMoveCardForLevel(m_runLevel + 1),
        generatedMoveCardForLevel(m_runLevel + 2),
        generatedMoveCardForLevel(m_runLevel + 3)
    };

    std::vector<std::string> weaponChoices;
    std::vector<std::string> weaponDetails;
    for (int choice : m_pendingRewardWeaponChoices) {
        weaponChoices.push_back(weaponNameFromChoice(choice));
        weaponDetails.push_back(weaponRewardDetail(choice));
    }

    std::vector<std::string> componentChoices;
    std::vector<std::string> componentDetails;
    for (int choice : m_pendingRewardComponentChoices) {
        componentChoices.push_back(supportNameFromId(choice));
        componentDetails.push_back(supportDetailFromId(choice));
    }

    std::vector<std::string> moveChoices;
    std::vector<std::string> moveDetails;
    for (const auto& move : m_pendingRewardMoveChoices) {
        moveChoices.push_back(move.name);
        moveDetails.push_back(move.description);
    }

    std::string title;
    std::string body;
    std::string buttonLabel;

    if (isInitialLoadout) {
        title = "Choose Your Loadout";
        body = "Select 1 weapon, 1 component, and 1 move before entering the first sector.";
        buttonLabel = "Begin Run";
    } else {
        title = "Level " + std::to_string(std::max(1, m_runLevel)) + " Clear";
        body = "Choose a new weapon, component, and move to strengthen your chassis for the next sector. "
               "Stored gear: " + std::to_string(m_inventoryEntries.size()) + ".";
        buttonLabel = (m_runLevel >= kMaxRunLevels) ? "Finish Run" : "Continue";
    }

    m_uiView->showRewardScreen(
        title,
        body,
        weaponChoices,
        weaponDetails,
        componentChoices,
        componentDetails,
        moveChoices,
        moveDetails,
        buttonLabel);
    m_rewardScreenOpen = true;
}

void GameController::openDefeatScreen() {
    m_defeatScreenOpen = true;
    m_uiView->showIntermission(
        "Pilot Down",
        "The chassis was destroyed before you escaped assimilation. Rebuild your loadout and try again.",
        "Restart Run");
}

void GameController::applyRewardSelections() {
    std::optional<int> equippedComponentId;
    std::optional<ProgramCard> equippedMoveCard;

    if (const auto selectedWeapon = m_uiView->getSelectedRewardWeapon()) {
        if (*selectedWeapon >= 0 && *selectedWeapon < static_cast<int>(m_pendingRewardWeaponChoices.size())) {
            const int choice = m_pendingRewardWeaponChoices[static_cast<size_t>(*selectedWeapon)];
            const int selectedRound = std::max(1, m_runLevel);
            const bool isNewWeapon = std::find(m_ownedWeaponChoices.begin(), m_ownedWeaponChoices.end(), choice) == m_ownedWeaponChoices.end();
            if (m_initialRewardSelectionOpen) {
                m_equippedWeaponChoice = choice;
            }
            if (isNewWeapon) {
                m_ownedWeaponChoices.push_back(choice);
                m_weaponSelectionRounds[choice] = selectedRound;
                m_inventoryEntries.push_back({weaponNameFromChoice(choice), weaponRewardDetail(choice)});
            } else {
                m_weaponSelectionRounds[choice] = std::max(m_weaponSelectionRounds[choice], selectedRound);
            }
        }
    }
    if (const auto selectedComponent = m_uiView->getSelectedRewardComponent()) {
        if (*selectedComponent >= 0 && *selectedComponent < static_cast<int>(m_pendingRewardComponentChoices.size())) {
            const int choice = m_pendingRewardComponentChoices[static_cast<size_t>(*selectedComponent)];
            if (m_initialRewardSelectionOpen) {
                equippedComponentId = choice;
            } else {
                m_equippedSupportIds.push_back(choice);
                m_equippedMoveIds.push_back(choice);
            }
            const bool isNewSupport = std::find(m_ownedSupportIds.begin(), m_ownedSupportIds.end(), choice) == m_ownedSupportIds.end();
            if (isNewSupport) {
                m_ownedSupportIds.push_back(choice);
                m_inventoryEntries.push_back({supportNameFromId(choice), supportDetailFromId(choice)});
            }
        }
    }
    for (int selectedMove : m_uiView->getSelectedRewardMoves()) {
        if (selectedMove >= 0 && selectedMove < static_cast<int>(m_pendingRewardMoveChoices.size())) {
            const ProgramCard& move = m_pendingRewardMoveChoices[static_cast<size_t>(selectedMove)];
            if (m_initialRewardSelectionOpen) {
                equippedMoveCard = move;
            } else {
                m_runMoveCards.push_back(move);
            }
        }
    }

    if (m_initialRewardSelectionOpen) {
        m_equippedSupportIds.clear();
        m_equippedMoveIds.clear();
        if (equippedComponentId.has_value()) {
            m_equippedSupportIds.push_back(*equippedComponentId);
            m_equippedMoveIds.push_back(*equippedComponentId);
        }

        m_selectedItemIds.clear();
        m_runMoveCards = defaultRunMoveCards();
        if (equippedMoveCard.has_value()) {
            m_runMoveCards.push_back(*equippedMoveCard);
        }
    }

    m_uiView->clearRewardSelections();
}

void GameController::advanceAfterReward() {
    m_rewardScreenOpen = false;
    m_initialRewardSelectionOpen = false;
    m_uiView->setState(0);
    if (m_runLevel >= kMaxRunLevels) {
        m_uiView->showIntermission(
            "Run Complete",
            "Congrats, you have survived.... for now",
            "Back To Title");
        return;
    }

    loadRunLevel(m_runLevel + 1);
}

void GameController::resetRunProgress(bool returnToTitleScreen) {
    m_inventoryEntries.clear();
    m_equippedWeaponChoice = 0;
    m_ownedWeaponChoices.clear();
    m_ownedSupportIds.clear();
    m_equippedSupportIds.clear();
    m_equippedMoveIds.clear();
    m_selectedItemIds.clear();
    m_runMoveCards = defaultRunMoveCards();
    m_weaponSelectionRounds.clear();
    m_rewardScreenOpen = false;
    m_initialRewardSelectionOpen = false;
    m_defeatScreenOpen = false;
    m_turnAnimationActive = false;
    m_turnResolvedDuringAnimation = false;
    m_turnAnimationResolveMs = 0;
    m_turnAnimationPath.clear();
    m_enemyTurnAnimations.clear();

    loadRunLevel(m_startLevelIndex);
    if (m_enableLoadoutUI) {
        if (returnToTitleScreen) {
            m_uiView->showTitleScreen();
        } else {
            openRewardScreen(true);
        }
    } else {
        m_uiView->setState(0);
    }
}

void GameController::skipCurrentLevelToReward() {
    if (!m_model || m_rewardScreenOpen || m_uiView->isBlocking()) return;
    if (m_model->getState() != util::GameState::IN_GAME) return;
    openRewardScreen();
}

void GameController::run() {
    if (!initSDL()) return;
    SDL_SetWindowTitle(m_window, "Iron Circuit");
    m_model = std::make_unique<GameModel>();
    m_humanView = std::make_unique<HumanView>();
    m_uiView = std::make_unique<UIView>(m_model.get());
    m_renderer = std::make_unique<Renderer>(m_sdlRenderer);
    m_audioManager = std::make_unique<AudioManager>();
    m_audioManager->init(m_model->getEventBus());
    m_audioManager->startAmbience();

    m_inventoryEntries.clear();
    m_runMoveCards = defaultRunMoveCards();
    loadRunLevel(m_startLevelIndex);
    if (m_enableLoadoutUI) {
        m_uiView->showTitleScreen();
    } else {
        m_uiView->setState(0);
    }

    while (m_running) {
        Uint32 frameStart = SDL_GetTicks();
        
        pollEvents();
        update();
        render();

        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 16) {
            SDL_Delay(16 - frameTime);
        }
    }

    shutdownSDL();
}

bool GameController::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
    m_window = SDL_CreateWindow("Iron Circuit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                util::layout::WindowWidth, util::layout::WindowHeight,
                                SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!m_window) return false;
    m_sdlRenderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_sdlRenderer) return false;
    return true;
}

void GameController::shutdownSDL() {
    m_audioManager.reset();
    m_renderer.reset();
    m_humanController.reset();
    m_uiView.reset();
    m_humanView.reset();
    m_model.reset();
    if (m_sdlRenderer) { SDL_DestroyRenderer(m_sdlRenderer); m_sdlRenderer = nullptr; }
    if (m_window) { SDL_DestroyWindow(m_window); m_window = nullptr; }
    SDL_Quit();
}

void GameController::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) m_running = false;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            m_running = false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F8 && m_audioManager) {
            m_audioManager->playDebugProbe();
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_n) {
            skipCurrentLevelToReward();
        }
        m_humanView->handleSDLEvent(event);
        m_uiView->handleSDLEvent(event);
    }
}

void GameController::update() {
    if (m_uiView->consumeTitleStart()) {
        if (m_enableLoadoutUI) {
            // Show reward screen for initial loadout (1 weapon + 1 component + 1 move)
            openRewardScreen(true);
        } else {
            m_uiView->setState(0);
        }
    }

    if (m_uiView->consumeRewardContinue()) {
        if (m_initialRewardSelectionOpen) {
            applyRewardSelections();
            m_rewardScreenOpen = false;
            m_initialRewardSelectionOpen = false;
            m_uiView->setState(0);
            applyCurrentLoadout();
            m_humanController->syncCardsToView();
            m_humanView->clearSelection();
        } else {
            applyRewardSelections();
            advanceAfterReward();
        }
        return;
    }

    if (m_uiView->consumeLoadoutConfirmed()) {
        const int requestedIndex = m_uiView->getLoadoutChoice().value_or(0);
        int requestedWeapon = m_ownedWeaponChoices.empty() ? 0
                             : m_ownedWeaponChoices[static_cast<size_t>(requestedIndex % static_cast<int>(m_ownedWeaponChoices.size()))];
        m_equippedWeaponChoice = requestedWeapon;
        m_ownedWeaponChoices = {requestedWeapon};
        m_equippedSupportIds = m_uiView->getLoadoutComponentChoices();
        m_ownedSupportIds = m_equippedSupportIds;
        m_equippedMoveIds = m_equippedSupportIds;
        m_selectedItemIds = m_uiView->getSelectedLoadoutItemIds();
        applyCurrentLoadout();
        m_humanController->syncCardsToView();
        m_humanView->clearSelection();
        m_uiView->clearLoadoutSelections();
        m_uiView->setState(0);
    }

    if (m_uiView->consumeContinueChoice()) {
        resetRunProgress(!m_defeatScreenOpen);
        return;
    }

    if (!m_uiView->isBlocking() && m_model->getState() == util::GameState::VICTORY && !m_rewardScreenOpen) {
        openRewardScreen();
    } else if (!m_uiView->isBlocking() && m_model->getState() == util::GameState::DEFEAT && !m_defeatScreenOpen) {
        openDefeatScreen();
        return;
    }

    if (m_uiView->isBlocking()) return;
    if (m_model->getState() != util::GameState::IN_GAME) return;

    if (m_turnAnimationActive) {
        updateTurnAnimation();
        return;
    }

    if (auto* player = m_model->getPlayer()) {
        const int cycleDir = m_humanView->consumeWeaponCycleRequest();
        if (cycleDir != 0 && !m_ownedWeaponChoices.empty()) {
            auto it = std::find(m_ownedWeaponChoices.begin(), m_ownedWeaponChoices.end(), m_equippedWeaponChoice);
            int index = (it != m_ownedWeaponChoices.end())
                ? static_cast<int>(std::distance(m_ownedWeaponChoices.begin(), it))
                : 0;
            index += (cycleDir > 0) ? 1 : -1;
            if (index < 0) {
                index = static_cast<int>(m_ownedWeaponChoices.size()) - 1;
            } else if (index >= static_cast<int>(m_ownedWeaponChoices.size())) {
                index = 0;
            }
            m_equippedWeaponChoice = m_ownedWeaponChoices[static_cast<size_t>(index)];
            player->getBody()->equipComponent(makePrototypeWeapon(
                m_equippedWeaponChoice,
                weaponSelectionRound(m_equippedWeaponChoice, m_weaponSelectionRounds)), RobotBody::kWeaponSlot);
            player->resetActiveWeaponSlot();
        }
        if (const auto requestedSlot = m_humanView->consumeRequestedWeaponSlot()) {
            player->setActiveWeaponSlot(*requestedSlot);
        }
    }

    collectAndSubmitActions();
    if (!m_model->allActionsReady()) {
        return;
    }
    if (!m_turnAnimationActive) {
        beginTurnAnimation();
    }
}

void GameController::collectAndSubmitActions() {
    m_humanController->syncCardsToView();
    m_humanController->produceAndSubmitAction();
    m_model->collectEnemyActions();
}

void GameController::beginTurnAnimation() {
    if (!m_model || !m_model->allActionsReady()) return;
    Level* level = m_model->getLevel();
    PlayerRobot* player = m_model->getPlayer();
    if (!level || !player) return;

    m_turnAnimationPath.clear();
    m_enemyTurnAnimations.clear();
    m_turnAnimationStart = player->getPos();
    m_turnAnimationFacing = player->getFacing();
    if (const auto& pendingPlayerAction = m_model->getPendingPlayerAction()) {
        if (pendingPlayerAction->direction != util::Direction::NONE) {
            m_turnAnimationFacing = pendingPlayerAction->direction;
        }
        const PreviewPathResult preview = computePreviewPath(
            level, m_turnAnimationStart, m_turnAnimationFacing, pendingPlayerAction->card);
        m_turnAnimationPath = preview.tiles;
        if (m_audioManager) {
            const Weapon* weapon = player->getActiveWeapon();
            const bool ranged = weapon && weapon->getWeaponType() == util::WeaponType::RANGED;
            m_audioManager->playPlayerAttack(ranged);
        }
    }
    std::vector<EnemyRobot*> enemies = level->getEnemies();
    bool hasEnemyAttack = false;
    bool hasRangedEnemyAttack = false;
    const int visibleRadiusX = util::layout::WorldViewportW / (2 * util::layout::CellSize) + 1;
    const int visibleRadiusY = util::layout::WorldViewportH / (2 * util::layout::CellSize) + 1;
    const auto isVisibleTile = [&](const util::Vector2i& tile) {
        return std::abs(tile.x - m_turnAnimationStart.x) <= visibleRadiusX &&
               std::abs(tile.y - m_turnAnimationStart.y) <= visibleRadiusY;
    };

    for (const auto& [enemyId, action] : m_model->getPendingEnemyActions()) {
        EnemyRobot* matchedEnemy = nullptr;
        for (EnemyRobot* enemy : enemies) {
            if (enemy && enemy->getId() == enemyId) {
                matchedEnemy = enemy;
                break;
            }
        }
        if (!matchedEnemy) continue;
        EnemyTurnAnimation enemyAnim;
        enemyAnim.enemyId = matchedEnemy->getId();
        enemyAnim.startPos = matchedEnemy->getPos();
        enemyAnim.targetPos = {action.targetX, action.targetY};
        enemyAnim.moveType = action.moveType;
        if (action.moveType == util::MoveType::ATTACK) {
            hasEnemyAttack = true;
            if (matchedEnemy->getSpritePath().find("ranged") != std::string::npos) {
                hasRangedEnemyAttack = true;
            }
        }
        if (!isVisibleTile(enemyAnim.startPos) && !isVisibleTile(enemyAnim.targetPos))
            continue;
        m_enemyTurnAnimations.push_back(enemyAnim);
    }

    if (m_audioManager && hasEnemyAttack) {
        m_audioManager->playEnemyAttack(hasRangedEnemyAttack);
    }

    m_turnAnimationStartMs = SDL_GetTicks();
    m_turnResolvedDuringAnimation = false;
    m_turnAnimationResolveMs = 0;
    m_turnAnimationActive = true;
}

void GameController::updateTurnAnimation() {
    if (!m_turnAnimationActive || !m_model) return;
    const unsigned nowMs = SDL_GetTicks();
    const unsigned elapsedMs = nowMs - m_turnAnimationStartMs;
    const unsigned playerMoveDurationMs = static_cast<unsigned>(util::animation::MoveStepMs) *
                                    static_cast<unsigned>(m_turnAnimationPath.size());
    const unsigned playerPhaseDurationMs = playerMoveDurationMs + static_cast<unsigned>(util::animation::AttackJiggleMs);
    const unsigned perEnemyPhaseDurationMs = static_cast<unsigned>(
        std::max({util::animation::MoveStepMs, util::animation::AttackJiggleMs, util::animation::EnemyPhaseMs}));
    const unsigned enemyPhaseDurationMs = m_enemyTurnAnimations.empty() ? 0u : perEnemyPhaseDurationMs;
    const unsigned resolveAtMs = playerPhaseDurationMs + enemyPhaseDurationMs;
    const unsigned totalDurationMs = resolveAtMs + (m_enemyTurnAnimations.empty() ? 0u : kPostResolveLabelMs);
    if (!m_turnResolvedDuringAnimation && elapsedMs >= resolveAtMs) {
        m_model->executeTurn();
        m_turnResolvedDuringAnimation = true;
        m_turnAnimationResolveMs = nowMs;
    }
    if (elapsedMs < totalDurationMs) {
        return;
    }

    if (Level* level = m_model->getLevel()) {
        for (EnemyRobot* enemy : level->getEnemies()) {
            if (enemy) enemy->clearRecentActionLabel();
        }
    }
    m_turnAnimationActive = false;
    m_turnResolvedDuringAnimation = false;
    m_turnAnimationResolveMs = 0;
    m_turnAnimationPath.clear();
    m_enemyTurnAnimations.clear();
}

void GameController::render() {
    m_renderer->clear();
    m_renderer->getDrawQueue()->clear();

    Level* level = m_model->getLevel();
    PlayerRobot* player = m_model->getPlayer();
    if (level && player) {
        m_renderer->setCameraFocus(player->getPos(), level->getWidth(), level->getHeight());
        level->submitDrawRequests(m_renderer->getDrawQueue(), !m_turnAnimationActive);
        const int mx = m_humanView->getLastMouseX();
        const int my = m_humanView->getLastMouseY();
        if (mx >= util::layout::WorldViewportX && mx < util::layout::WorldViewportX + util::layout::WorldViewportW &&
            my >= util::layout::WorldViewportY && my < util::layout::WorldViewportY + util::layout::WorldViewportH) {
            const int worldX = mx - util::layout::WorldViewportX + m_renderer->cameraOffsetX();
            const int worldY = my - util::layout::WorldViewportY + m_renderer->cameraOffsetY();
            const util::Vector2i tile{worldX / util::layout::CellSize, worldY / util::layout::CellSize};
            Robot* hovered = level->getRobotAt(tile);
            if (auto* enemy = dynamic_cast<EnemyRobot*>(hovered)) {
                m_humanView->setWorldHoverText(enemy->hoverDescription());
            } else if (hovered == player) {
                m_humanView->setWorldHoverText("player");
            } else {
                m_humanView->setWorldHoverText("");
            }
        } else {
            m_humanView->setWorldHoverText("");
        }
    }

    const auto selectedCard = m_humanView->getSelectedCard();
    const util::Direction previewFacing = m_humanView->getPendingDirection().value_or(player ? player->getFacing() : util::Direction::RIGHT);

    if (player) {
        const util::Direction originalFacing = player->getFacing();
        const util::Vector2i originalPos = player->getPos();
        int animOffsetX = 0;
        int animOffsetY = 0;
        util::Direction drawFacing = previewFacing;

        if (m_turnAnimationActive && !m_turnResolvedDuringAnimation) {
            drawFacing = m_turnAnimationFacing;
            const unsigned nowMs = SDL_GetTicks();
            const unsigned elapsedMs = nowMs - m_turnAnimationStartMs;
            const unsigned moveDurationMs = static_cast<unsigned>(util::animation::MoveStepMs) *
                                            static_cast<unsigned>(m_turnAnimationPath.size());
            const unsigned playerPhaseDurationMs = moveDurationMs + static_cast<unsigned>(util::animation::AttackJiggleMs);
            const int cell = util::layout::CellSize;

            if (!m_turnAnimationPath.empty() && elapsedMs < moveDurationMs && util::animation::MoveStepMs > 0) {
                const unsigned stepIndex = elapsedMs / static_cast<unsigned>(util::animation::MoveStepMs);
                const unsigned localMs = elapsedMs % static_cast<unsigned>(util::animation::MoveStepMs);
                const float t = static_cast<float>(localMs) / static_cast<float>(util::animation::MoveStepMs);
                const util::Vector2i from = (stepIndex == 0)
                    ? m_turnAnimationStart
                    : m_turnAnimationPath[stepIndex - 1];
                const util::Vector2i to = m_turnAnimationPath[stepIndex];
                animOffsetX = static_cast<int>(std::round(((to.x - from.x) * t) * cell));
                animOffsetY = static_cast<int>(std::round(((to.y - from.y) * t) * cell));
                player->setPos(from);
            } else if (elapsedMs < playerPhaseDurationMs) {
                if (!m_turnAnimationPath.empty()) {
                    player->setPos(m_turnAnimationPath.back());
                }
                const unsigned jiggleElapsed = (elapsedMs > moveDurationMs) ? (elapsedMs - moveDurationMs) : 0u;
                const float jiggleT = util::animation::AttackJiggleMs > 0
                    ? static_cast<float>(jiggleElapsed) / static_cast<float>(util::animation::AttackJiggleMs)
                    : 1.0f;
                const float cycle = std::sin(jiggleT * 6.2831853f);
                const int jigglePx = static_cast<int>(std::round(cycle * 8.0f));
                const util::Vector2i facingOffset = util::directionOffset(drawFacing);
                animOffsetX = facingOffset.x * jigglePx;
                animOffsetY = facingOffset.y * jigglePx;
            } else {
                if (!m_turnAnimationPath.empty()) {
                    player->setPos(m_turnAnimationPath.back());
                }
            }
        }

        player->setFacing(drawFacing);
        player->submitDrawRequests(m_renderer->getDrawQueue(), animOffsetX, animOffsetY);
        player->setFacing(originalFacing);
        player->setPos(originalPos);

        if (!m_turnAnimationActive && m_humanView->getPendingDirection().has_value()) {
            pushFacingPreview(m_renderer->getDrawQueue(), player->getPos(), previewFacing);
        }
    }

    if (level && m_turnAnimationActive) {
        std::vector<EnemyRobot*> enemies = level->getEnemies();
        std::unordered_map<int, std::pair<int, int>> enemyOffsets;
        const unsigned nowMs = SDL_GetTicks();
        const unsigned elapsedMs = nowMs - m_turnAnimationStartMs;
        const unsigned playerMoveDurationMs = static_cast<unsigned>(util::animation::MoveStepMs) *
                                              static_cast<unsigned>(m_turnAnimationPath.size());
        const unsigned playerPhaseDurationMs = playerMoveDurationMs + static_cast<unsigned>(util::animation::AttackJiggleMs);
        const unsigned enemyPhaseElapsedMs = (elapsedMs > playerPhaseDurationMs) ? (elapsedMs - playerPhaseDurationMs) : 0u;
        const unsigned perEnemyPhaseDurationMs = static_cast<unsigned>(std::max(util::animation::MoveStepMs, util::animation::AttackJiggleMs));
        const int cell = util::layout::CellSize;

        if (!m_turnResolvedDuringAnimation) {
            const unsigned localEnemyElapsedMs = std::min(enemyPhaseElapsedMs, perEnemyPhaseDurationMs);
            for (const EnemyTurnAnimation& anim : m_enemyTurnAnimations) {
                if (anim.enemyId < 0) continue;
                int offsetX = 0;
                int offsetY = 0;

                const bool enemyActive = localEnemyElapsedMs < perEnemyPhaseDurationMs;
                if (anim.moveType == util::MoveType::ATTACK && enemyActive) {
                    pushEnemyAttackTelegraph(m_renderer->getDrawQueue(), level, anim.startPos, anim.targetPos, 8970 + anim.enemyId * 8);
                }
                if (localEnemyElapsedMs >= perEnemyPhaseDurationMs) {
                    if (anim.moveType == util::MoveType::MOVE) {
                        offsetX = (anim.targetPos.x - anim.startPos.x) * cell;
                        offsetY = (anim.targetPos.y - anim.startPos.y) * cell;
                    }
                } else {
                    if (anim.moveType == util::MoveType::MOVE) {
                        const float t = util::animation::MoveStepMs > 0
                            ? std::min(1.0f, static_cast<float>(localEnemyElapsedMs) /
                                                static_cast<float>(util::animation::MoveStepMs))
                            : 1.0f;
                        offsetX = static_cast<int>(std::round((anim.targetPos.x - anim.startPos.x) * t * cell));
                        offsetY = static_cast<int>(std::round((anim.targetPos.y - anim.startPos.y) * t * cell));
                    } else if (anim.moveType == util::MoveType::ATTACK) {
                        const util::Direction attackDir = directionToward(anim.startPos, anim.targetPos);
                        const util::Vector2i dirOffset = util::directionOffset(attackDir);
                        const float t = util::animation::AttackJiggleMs > 0
                            ? std::min(1.0f, static_cast<float>(localEnemyElapsedMs) /
                                                static_cast<float>(util::animation::AttackJiggleMs))
                            : 1.0f;
                        const float cycle = std::sin(t * 6.2831853f);
                        const int jigglePx = static_cast<int>(std::round(cycle * 7.0f));
                        offsetX = dirOffset.x * jigglePx;
                        offsetY = dirOffset.y * jigglePx;
                    }
                }
                enemyOffsets[anim.enemyId] = {offsetX, offsetY};
            }
        }

        for (EnemyRobot* enemy : enemies) {
            if (!enemy) continue;
            const util::Vector2i originalPos = enemy->getPos();
            const auto found = enemyOffsets.find(enemy->getId());
            const int offsetX = (found != enemyOffsets.end()) ? found->second.first : 0;
            const int offsetY = (found != enemyOffsets.end()) ? found->second.second : 0;
            enemy->submitDrawRequests(m_renderer->getDrawQueue(), offsetX, offsetY);
            enemy->setPos(originalPos);
        }
    }

    if (level && player && selectedCard && !m_turnAnimationActive) {
        const PreviewPathResult preview = computePreviewPath(level, player->getPos(), previewFacing, *selectedCard);
        int stepZ = 9000;
        for (const auto& tile : preview.tiles) {
            pushPreviewTile(m_renderer->getDrawQueue(), tile, 0x8838D97Au, stepZ++);
        }

        if (selectedCard->moveType == util::MoveType::ATTACK || selectedCard->moveType == util::MoveType::MOVE) {
            if (const Weapon* weapon = findEquippedAttackWeapon(player)) {
                const auto attackTiles = weapon->getAttackTiles(preview.finalPos, previewFacing, level);
                for (const auto& tile : attackTiles) {
                    if (!level->isInBounds(tile)) continue;
                    pushPreviewTile(m_renderer->getDrawQueue(), tile, 0x88FF4D4Du, stepZ++);
                }
            }
        }
    }

    if (player) {
        m_humanView->setPlayerStats(player->getHp(), player->getMaxHp(),
                                   player->getHeat(), player->getMaxHeat());
        const auto [weaponName, weaponDescription] = getWeaponSummary(findEquippedAttackWeapon(player));
        m_humanView->setWeaponSummary(weaponName, weaponDescription);
        m_humanView->setEquipmentEntries(getEquipmentEntries(player, m_ownedWeaponChoices, m_ownedSupportIds, m_equippedWeaponChoice, m_weaponSelectionRounds));
    }
    if (m_model) {
        const auto& logEntries = m_model->getCombatLogEntries();
        m_humanView->setCombatLog(std::vector<std::string>(logEntries.begin(), logEntries.end()));
    }
    m_humanView->submitDrawRequests(m_renderer->getDrawQueue());
    m_uiView->submitDrawRequests(m_renderer->getDrawQueue());
    m_renderer->renderAll();
    m_renderer->present();
}
