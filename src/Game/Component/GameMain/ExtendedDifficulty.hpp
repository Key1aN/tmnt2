#pragma once

#include "GameTypes.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCModFeatures.hpp"
#endif /* defined(TARGET_PC) */

#if defined(TMNT2_DEBUG_TOOLS)
#include "DebugDifficulty.hpp"
#endif /* defined(TMNT2_DEBUG_TOOLS) */


namespace EXTENDEDDIFFICULTY
{
    inline bool IsEnabled(void)
    {
#if defined(TARGET_PC)
        return CPCModFeatures::IsBrutalDifficultiesEnabled();
#else /* defined(TARGET_PC) */
        return false;
#endif /* defined(TARGET_PC) */
    };


    inline float GetBaseEnemyHPScale(GAMETYPES::DIFFICULTY difficulty)
    {
        if (!IsEnabled())
            return 1.00f;

        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 1.50f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 2.00f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 2.50f;
        default:                              return 1.00f;
        };
    };


    inline float GetBasePlayerDamageReceivedScale(GAMETYPES::DIFFICULTY difficulty)
    {
        if (!IsEnabled())
            return 1.00f;

        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 2.50f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 4.00f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 6.00f;
        default:                              return 1.00f;
        };
    };


    inline float GetBaseEPBAggressionScale(GAMETYPES::DIFFICULTY difficulty)
    {
        if (!IsEnabled())
            return 1.00f;

        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_HARD:      return 1.05f;
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 1.15f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 1.25f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 1.35f;
        default:                              return 1.00f;
        };
    };


    inline float GetBaseAttackIntervalScale(GAMETYPES::DIFFICULTY difficulty)
    {
        if (!IsEnabled())
            return 1.00f;

        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 0.75f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 0.50f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 0.30f;
        default:                              return 1.00f;
        };
    };


    inline float GetBaseKnockBackThresholdScale(GAMETYPES::DIFFICULTY difficulty)
    {
        if (!IsEnabled())
            return 1.00f;

        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_VERY_HARD: return 0.75f;
        case GAMETYPES::DIFFICULTY_EXTREME:   return 0.50f;
        case GAMETYPES::DIFFICULTY_SOULS_LIKE:return 0.30f;
        default:                              return 1.00f;
        };
    };


    inline float GetPlayerDamageDealtScale(GAMETYPES::DIFFICULTY difficulty)
    {
#if defined(TMNT2_DEBUG_TOOLS)
        return CDebugDifficulty::GetValue(CDebugDifficulty::SETTING_PLAYER_DAMAGE_DEALT, difficulty);
#else /* defined(TMNT2_DEBUG_TOOLS) */
        return 1.00f;
#endif /* defined(TMNT2_DEBUG_TOOLS) */
    };


    inline float GetEnemyHPScale(GAMETYPES::DIFFICULTY difficulty)
    {
#if defined(TMNT2_DEBUG_TOOLS)
        return CDebugDifficulty::GetValue(CDebugDifficulty::SETTING_ENEMY_HP, difficulty);
#else /* defined(TMNT2_DEBUG_TOOLS) */
        return GetBaseEnemyHPScale(difficulty);
#endif /* defined(TMNT2_DEBUG_TOOLS) */
    };


    inline float GetPlayerDamageReceivedScale(GAMETYPES::DIFFICULTY difficulty)
    {
#if defined(TMNT2_DEBUG_TOOLS)
        return CDebugDifficulty::GetValue(CDebugDifficulty::SETTING_PLAYER_DAMAGE_RECEIVED, difficulty);
#else /* defined(TMNT2_DEBUG_TOOLS) */
        return GetBasePlayerDamageReceivedScale(difficulty);
#endif /* defined(TMNT2_DEBUG_TOOLS) */
    };


    inline float GetEPBAggressionScale(GAMETYPES::DIFFICULTY difficulty)
    {
#if defined(TMNT2_DEBUG_TOOLS)
        return CDebugDifficulty::GetValue(CDebugDifficulty::SETTING_ATTACK_FREQUENCY, difficulty);
#else /* defined(TMNT2_DEBUG_TOOLS) */
        return GetBaseEPBAggressionScale(difficulty);
#endif /* defined(TMNT2_DEBUG_TOOLS) */
    };


    inline float GetAttackIntervalScale(GAMETYPES::DIFFICULTY difficulty)
    {
#if defined(TMNT2_DEBUG_TOOLS)
        return CDebugDifficulty::GetValue(CDebugDifficulty::SETTING_ATTACK_INTERVAL, difficulty);
#else /* defined(TMNT2_DEBUG_TOOLS) */
        return GetBaseAttackIntervalScale(difficulty);
#endif /* defined(TMNT2_DEBUG_TOOLS) */
    };


    inline float GetKnockBackThresholdScale(GAMETYPES::DIFFICULTY difficulty)
    {
#if defined(TMNT2_DEBUG_TOOLS)
        return CDebugDifficulty::GetValue(CDebugDifficulty::SETTING_KNOCKBACK_THRESHOLD, difficulty);
#else /* defined(TMNT2_DEBUG_TOOLS) */
        return GetBaseKnockBackThresholdScale(difficulty);
#endif /* defined(TMNT2_DEBUG_TOOLS) */
    };
};
