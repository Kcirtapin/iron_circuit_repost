#ifndef IRON_CIRCUIT_UTIL_GAMECONFIG_H
#define IRON_CIRCUIT_UTIL_GAMECONFIG_H

#include "util/Types.h"
#include <vector>

namespace util {
    inline PlayerConfig loadPlayerConfig() { return PlayerConfig{}; }
    inline std::vector<EnemyConfig> loadEnemiesConfig() { return {}; }

    namespace layout {
        constexpr int LogicalWidth = 1600;
        constexpr int LogicalHeight = 900;
        constexpr int WindowWidth = 1280;
        constexpr int WindowHeight = 720;

        constexpr int WorldViewportX = 32;
        constexpr int WorldViewportY = 32;
        constexpr int WorldViewportW = 1040;
        constexpr int WorldViewportH = 712;

        constexpr int SidebarX = 1104;
        constexpr int SidebarY = 32;
        constexpr int SidebarW = 464;
        constexpr int SidebarH = 712;

        constexpr int BottomBarX = 32;
        constexpr int BottomBarY = 776;
        constexpr int BottomBarW = 1536;
        constexpr int BottomBarH = 92;

        constexpr int CellSize = 96;
        constexpr int TileBorder = 2;
        constexpr int RobotPad = 10;
        constexpr int HealthBarHeight = 8;
        constexpr int CardSlots = 5;
    }

    namespace animation {
        constexpr int MoveStepMs = 170;
        constexpr int AttackJiggleMs = 240;
        constexpr int EnemyPhaseMs = 360;
    }

    namespace generation {
        constexpr int MinGeneratedEnemyCount = 24;
        constexpr int BaseGeneratedEnemyCount = 40;
        constexpr int GeneratedEnemyCountPerLevel = 12;
        constexpr int ClusterRoomModulo = 2;
    }
}

#endif // IRON_CIRCUIT_UTIL_GAMECONFIG_H
