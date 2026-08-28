#pragma once

#include "GameTypes.hpp"


namespace EXTENDEDDIFFICULTY
{
    inline float GetEnemyHPScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 1.50f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 2.00f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 2.50f;
        default:                              return 1.00f;
        };
    };


    inline float GetPlayerDamageReceivedScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 2.50f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 4.00f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 6.00f;
        default:                              return 1.00f;
        };
    };


    inline float GetEPBAggressionScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_HARD:      return 1.05f;
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 1.15f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 1.25f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 1.35f;
        default:                              return 1.00f;
        };
    };


    inline float GetAttackIntervalScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 0.75f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 0.50f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 0.30f;
        default:                              return 1.00f;
        };
    };


    inline float GetKnockBackThresholdScale(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 0.75f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 0.50f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 0.30f;
        default:                              return 1.00f;
        };
    };
};
