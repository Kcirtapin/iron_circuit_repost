/**
 * Iron Circuit â€” entry point and combat test.
 * Combat test runs once before the game loop: one player sword attack turn
 * is executed and we assert the enemy took damage.
 */
#include "controller/GameController.h"
#include "model/GameModel.h"
#include "model/Level.h"
#include "model/PlayerRobot.h"
#include "model/EnemyRobot.h"
#include "model/Action.h"
#include "model/ProgramCard.h"
#include "model/Weapon.h"
#include "util/Types.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

static bool runCombatTest()
{
    std::cout << "Combat test: load level, submit player sword attack, execute turn...\n";
    auto model = std::make_unique<GameModel>();
    model->loadLevel(0); // 0 = defaults (single enemy) for reproducible test

    Level *level = model->getLevel();
    PlayerRobot *player = model->getPlayer();
    assert(level != nullptr);
    assert(player != nullptr);
    auto enemies = level->getEnemies();
    assert(enemies.size() == 1u);
    EnemyRobot *enemy = enemies[0];
    int enemyHpBefore = enemy->getHp();

    // Player at (1,3) facing RIGHT, enemy at (4,3).
    // Use the Sword card to attack; first move adjacent, then attack.
    // Submit a "Step" move toward enemy first.
    ProgramCard stepCard;
    stepCard.name = "Step";
    stepCard.moveType = util::MoveType::MOVE;
    stepCard.range = 1;
    stepCard.path = {util::RelativeDir::FORWARD};
    PlayerAction moveAction;
    moveAction.card = stepCard;
    moveAction.direction = util::Direction::RIGHT;
    moveAction.valid = true;
    model->submitPlayerAction(std::move(moveAction));
    model->collectEnemyActions();
    assert(model->allActionsReady());
    model->executeTurn(); // player moves to (2,3)

    // Move again to (3,3) â€” adjacent to enemy at (4,3)
    model->submitPlayerAction(PlayerAction{stepCard, util::Direction::RIGHT, true});
    model->collectEnemyActions();
    model->executeTurn(); // player moves to (3,3)

    // Now attack with Sword (hits tile in front = (4,3) = enemy)
    ProgramCard swordCard;
    swordCard.name = "Sword";
    swordCard.description = "Slash in front";
    swordCard.moveType = util::MoveType::ATTACK;
    swordCard.range = 1;
    PlayerAction attackAction;
    attackAction.card = swordCard;
    attackAction.direction = util::Direction::RIGHT;
    attackAction.valid = true;
    model->submitPlayerAction(std::move(attackAction));
    model->collectEnemyActions();
    assert(model->allActionsReady());
    model->executeTurn();

    enemies = level->getEnemies();
    bool enemyTookDamage = (enemies.empty() || enemies[0]->getHp() < enemyHpBefore);
    if (!enemyTookDamage && !enemies.empty())
        std::cerr << "Combat test: enemy hp before=" << enemyHpBefore << " after=" << enemies[0]->getHp() << "\n";
    assert(enemyTookDamage && "Combat test: enemy should have taken damage from sword attack");
    std::cout << "Combat test passed (enemy took damage or defeated).\n";
    return true;
}

int main(int argc, char **argv)
{
#if defined(__linux__)
    setenv("PULSE_LATENCY_MSEC", "60", 0);
#endif

    bool playOnly = false;
    bool prototypeTest = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--play" || arg == "-p")
            playOnly = true;
        if (arg == "--test")
            prototypeTest = true;
    }

    if (!playOnly && !prototypeTest)
    {
        bool testOk = runCombatTest();
        if (!testOk)
        {
            return 1;
        }
    }

    // Run the game: grid, player, enemies, HP/HEAT bars, card bar.
    // Keys: 1â€“5 select card, Arrow keys set facing, Enter confirm and execute turn.
    GameController controller;
    if (prototypeTest)
    {
        // Load the same prototype test level (index 0) used by runCombatTest,
        // but without the loadout UI overlay for a focused combat scenario.
        controller.setStartLevelIndex(0);
        controller.setEnableLoadoutUI(false);
    }
    controller.run();
    return 0;
}
