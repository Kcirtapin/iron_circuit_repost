#ifndef IRON_CIRCUIT_CONTROLLER_HUMANCONTROLLER_H
#define IRON_CIRCUIT_CONTROLLER_HUMANCONTROLLER_H

class PlayerRobot;
class GameModel;
class HumanView;

class HumanController {
public:
    HumanController(PlayerRobot* robot, GameModel* model, HumanView* view);

    bool isReady() const;
    void produceAndSubmitAction();
    void syncCardsToView();

private:
    PlayerRobot* m_robot;
    GameModel*   m_model;
    HumanView*   m_view;
};

#endif // IRON_CIRCUIT_CONTROLLER_HUMANCONTROLLER_H
