#include "model/EnemyRobot.h"
#include "controller/AIBehavior.h"
#include "render/DrawQueue.h"
#include "util/Types.h"
#include "util/GameConfig.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>

EnemyRobot::EnemyRobot(int id, util::Vector2i pos, std::weak_ptr<EventBus> eventBus)
    : Robot(id, pos, std::move(eventBus)) {}

EnemyRobot::~EnemyRobot() = default;

void EnemyRobot::setBehavior(std::unique_ptr<AIBehavior> behavior) {
    m_behavior = std::move(behavior);
}

void EnemyRobot::takeTurn() {
    if (m_behavior)
        m_telegraphedAction = m_behavior->tick();
}

bool EnemyRobot::consumeAttackDelayReady() {
    if (m_attackDelayTurns <= 1) return true;
    ++m_attackDelayCounter;
    if (m_attackDelayCounter >= m_attackDelayTurns) {
        m_attackDelayCounter = 0;
        return true;
    }
    return false;
}

float EnemyRobot::consumeDamageMultiplierOnHit() {
    if (!m_firstHitConsumed && m_firstHitMultiplier > 1.0f) {
        m_firstHitConsumed = true;
        return m_firstHitMultiplier;
    }
    return 1.0f;
}

void EnemyRobot::setSummonerConfig(int intervalTurns, int maxMinions) {
    m_summonIntervalTurns = std::max(0, intervalTurns);
    m_maxMinions = std::max(0, maxMinions);
    m_summonCounter = 0;
    m_spawnedMinions = 0;
}

bool EnemyRobot::consumeSummonReady() {
    if (m_summonIntervalTurns <= 0 || m_spawnedMinions >= m_maxMinions) return false;
    ++m_summonCounter;
    if (m_summonCounter >= m_summonIntervalTurns) {
        m_summonCounter = 0;
        return true;
    }
    return false;
}

void EnemyRobot::tickConditions() {
    if (m_hpRegenPerTurn > 0 && m_hp > 0) {
        heal(m_hpRegenPerTurn);
    }
}

void EnemyRobot::takeDamage(int d) {
    const float clampedReduction = std::clamp(m_damageReductionPct, 0.0f, 0.95f);
    const int reduced = static_cast<int>(std::round(static_cast<float>(d) * (1.0f - clampedReduction)));
    Robot::takeDamage(std::max(1, reduced));
}

std::string EnemyRobot::hoverDescription() const {
    std::string out = m_archetypeId.empty() ? "enemy" : m_archetypeId;
    if (m_damageReductionPct > 0.0f) out += " | armored";
    if (m_hpRegenPerTurn > 0) out += " | regen";
    if (m_explodeOnDeathDamage > 0) out += " | explosive";
    if (m_summonIntervalTurns > 0) out += " | summoner";
    return out;
}

void EnemyRobot::setRecentActionLabel(std::string text, int turnsToShow) {
    m_recentActionLabel = std::move(text);
    m_recentActionTurns = std::max(0, turnsToShow);
}

void EnemyRobot::clearRecentActionLabel() {
    m_recentActionLabel.clear();
    m_recentActionTurns = 0;
}

void EnemyRobot::decayRecentActionLabel() {
    if (m_recentActionTurns > 0) {
        --m_recentActionTurns;
        if (m_recentActionTurns == 0) {
            m_recentActionLabel.clear();
        }
    }
}

void EnemyRobot::submitDrawRequests(DrawQueue* queue) {
    submitDrawRequests(queue, 0, 0);
}

void EnemyRobot::submitDrawRequests(DrawQueue* queue, int pixelOffsetX, int pixelOffsetY) {
    if (!queue) return;
    const int cell = util::layout::CellSize;
    const int pad = util::layout::RobotPad;
    const int barH = util::layout::HealthBarHeight;
    const int px = m_pos.x * cell + pixelOffsetX;
    const int py = m_pos.y * cell + pixelOffsetY;

    DrawRequest body;
    body.layer = util::DrawLayer::ROBOTS;
    body.usePlaceholderShape = true;
    body.placeholderColor = 0xFFCC2200;
    body.texturePath = m_spritePath;
    body.dst.x = px + pad;
    body.dst.y = py + pad + barH + 6;
    body.dst.w = cell - pad * 2;
    body.dst.h = cell - pad * 2 - barH - 6;
    body.zOrder = 500 + m_id;
    queue->push(body);

    const int barW = cell - pad * 2;
    DrawRequest hpBg;
    hpBg.layer = util::DrawLayer::UI_WORLD;
    hpBg.usePlaceholderShape = true;
    hpBg.placeholderColor = 0xFF2A0000;
    hpBg.dst = { px + pad, py + pad, barW, barH };
    hpBg.zOrder = 600 + m_id;
    queue->push(hpBg);

    int hpFill = (m_maxHp > 0) ? (barW * m_hp / m_maxHp) : 0;
    if (hpFill > 0) {
        DrawRequest hpFg;
        hpFg.layer = util::DrawLayer::UI_WORLD;
        hpFg.usePlaceholderShape = true;
        hpFg.placeholderColor = 0xFFEE5500;
        hpFg.dst = { px + pad, py + pad, hpFill, barH };
        hpFg.zOrder = 601 + m_id;
        queue->push(hpFg);
    }

    DrawRequest hpText;
    hpText.layer = util::DrawLayer::UI_WORLD;
    hpText.hasText = true;
    hpText.textColor = 0xFFFFFFFFu;
    hpText.text = std::to_string(m_hp) + "/" + std::to_string(m_maxHp);
    hpText.dst.x = px + pad;
    hpText.dst.y = py - 22;
    hpText.dst.w = 120;
    hpText.zOrder = 602 + m_id;
    queue->push(hpText);

    if (m_telegraphedAction.moveType == util::MoveType::ATTACK || m_telegraphedAction.moveType == util::MoveType::MOVE) {
        DrawRequest tel;
        tel.layer = util::DrawLayer::EFFECTS;
        tel.usePlaceholderShape = true;
        tel.placeholderColor = (m_telegraphedAction.moveType == util::MoveType::ATTACK) ? 0xFFFFDD00 : 0xFF44AAFF;
        tel.dst = { px + cell - pad - 14, py + pad + 4, 14, 14 };
        tel.zOrder = 700 + m_id;
        queue->push(tel);
    }

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
        actionTextShadow.zOrder = 709 + m_id;
        queue->push(actionTextShadow);

        DrawRequest actionText;
        actionText.layer = util::DrawLayer::UI_WORLD;
        actionText.hasText = true;
        actionText.textColor = 0xFFFFF0AAu;
        actionText.textPointSize = 12;
        actionText.text = m_recentActionLabel;
        actionText.dst = {px + pad + jiggleX, py - 42, cell - pad * 2, 18};
        actionText.zOrder = 710 + m_id;
        queue->push(actionText);
    }

    DrawRequest idText;
    idText.layer = util::DrawLayer::UI_WORLD;
    idText.hasText = true;
    idText.textPointSize = 11;
    idText.textColor = m_awareOfPlayer ? 0xFFFFD38Au : 0xFFAAB2BFu;
    idText.text = m_archetypeId.empty() ? "enemy" : m_archetypeId;
    if (m_awareOfPlayer) idText.text += " !";
    idText.dst = {px + pad, py + cell - 14, cell - pad * 2, 12};
    idText.zOrder = 711 + m_id;
    queue->push(idText);
}
