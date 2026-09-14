#pragma once

#if defined(TMNT2_DEBUG_TOOLS)

#include "GameTypes.hpp"


class CDebugDifficulty
{
public:
    enum PRESET
    {
        PRESET_GAME = 0,
        PRESET_VANILLA,
        PRESET_VERY_HARD,
        PRESET_EXTREME,
        PRESET_SOULS_LIKE,

        PRESET_NUM,
        PRESET_CUSTOM,
    };

    enum SETTING
    {
        SETTING_PLAYER_DAMAGE_DEALT = 0,
        SETTING_ENEMY_HP,
        SETTING_PLAYER_DAMAGE_RECEIVED,
        SETTING_ATTACK_FREQUENCY,
        SETTING_GUARD_FREQUENCY,
        SETTING_PROJECTILE_FREQUENCY,
        SETTING_PROJECTILE_RANGE,
        SETTING_AI_THINKING,
        SETTING_AI_ACTIVITY,
        SETTING_AI_AWARENESS,
        SETTING_ATTACK_INTERVAL,
        SETTING_KNOCKBACK_THRESHOLD,

        SETTING_NUM,
    };

public:
    static void Initialize(void);
    static bool IsOverrideEnabled(void);
    static PRESET GetPreset(void);
    static const char* GetPresetName(void);
    static float GetValue(SETTING setting, GAMETYPES::DIFFICULTY difficulty);
    static float GetBuiltInValue(SETTING setting, GAMETYPES::DIFFICULTY difficulty);
    static float GetMinimum(SETTING setting);
    static float GetMaximum(SETTING setting);
    static float GetStep(SETTING setting);
    static void SetPreset(PRESET preset, GAMETYPES::DIFFICULTY difficulty);
    static void AdjustValue(SETTING setting, float delta, GAMETYPES::DIFFICULTY difficulty);
    static void ResetSetting(SETTING setting, GAMETYPES::DIFFICULTY difficulty);
    static void ResetAll(GAMETYPES::DIFFICULTY difficulty);
    static uint8 ScaleFrequency(
        int32 index,
        uint8 value,
        bool bBaseIncludesAggression,
        GAMETYPES::DIFFICULTY difficulty
    );
    static void RefreshLiveEnemies(void);

private:
    static void LoadBuiltInValues(GAMETYPES::DIFFICULTY difficulty);
    static void LoadPresetValues(PRESET preset);
    static float ClampSetting(SETTING setting, float value);

private:
    static PRESET m_preset;
    static bool m_bOverrideEnabled;
    static float m_afValue[SETTING_NUM];
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
