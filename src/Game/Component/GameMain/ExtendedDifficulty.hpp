#pragma once

#include "GameTypes.hpp"


namespace EXTENDEDDIFFICULTY
{
    inline float GetEnemyHPScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 1.25f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 1.50f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 1.75f;
        default:                              return 1.00f;
        };
    };


    inline float GetPlayerDamageReceivedScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 1.50f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 2.00f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 3.00f;
        default:                              return 1.00f;
        };
    };


    inline float GetEPBAggressionScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_HARD:      return 1.05f;
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 1.10f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 1.15f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 1.20f;
        default:                              return 1.00f;
        };
    };


    inline float GetAttackIntervalScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 0.85f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 0.70f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 0.50f;
        default:                              return 1.00f;
        };
    };


    inline float GetKnockBackThresholdScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 0.85f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 0.70f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 0.50f;
        default:                              return 1.00f;
        };
    };
};
