#include "controller/HumanController.h"
#include "model/GameModel.h"
#include "model/PlayerRobot.h"
#include "view/HumanView.h"
#include "model/Action.h"
#include "util/Types.h"

HumanController::HumanController(PlayerRobot* robot, GameModel* model, HumanView* view)
    : m_robot(robot), m_model(model), m_view(view) {}

bool HumanController::isReady() const {
    return m_view->hasConfirmed();
}

#include <iostream>

void HumanController::produceAndSubmitAction() {
    if (!m_view->hasConfirmed()) return;
    auto card = m_view->getSelectedCard();
    auto dir = m_view->getPendingDirection();
    
    if (!card) {
        std::cout << "[HumanController] Turn confirmed but NO card selected! Cancelling choice.\n";
        m_view->clearSelection();
        return;
    }

    // Direction is used as facing for path-based movement.
    // It's required for all cards that move or attack.
    if (!dir) dir = m_robot->getFacing();  // keep current facing if none specified

    std::cout << "[HumanController] Submitting PlayerAction: " << card->name 
              << " | Facing: " << static_cast<int>(dir.value()) << "\n";

    PlayerAction pa;
    pa.card = *card;
    pa.direction = dir.value();
    pa.valid = true;
    m_model->submitPlayerAction(std::move(pa));
    m_view->clearSelection();
}

void HumanController::syncCardsToView() {
    if (m_robot)
        m_view->setAvailableCards(m_robot->getAvailablePrograms());
}
