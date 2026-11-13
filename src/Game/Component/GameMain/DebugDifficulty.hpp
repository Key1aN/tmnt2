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

    // Combat Lab: complete override state, so the lab can restore exactly
    // what the user had in the Difficulty tab when it exits.
    struct SNAPSHOT
    {
        PRESET m_preset;
        bool m_bOverrideEnabled;
        float m_afValue[SETTING_NUM];
    };

    // What the number a setting produces means, for display.
    enum UNIT
    {
        UNIT_MULTIPLIER = 0,    // plain multiplier, no engine cap
        UNIT_PERCENT,           // result is a chance / rate in percent, capped
        UNIT_DISTANCE,          // result is a world distance, capped
        UNIT_VIEW,              // result is the AI view ratio, capped where it stops mattering
    };

    // Effective values on the enemies alive right now.
    struct LIVEINFO
    {
        bool  m_bValid;         // at least one live enemy has a non-zero base value
        int32 m_nEnemy;
        float m_fNowMin;        // lowest / highest non-zero effective value
        float m_fNowMax;
        float m_fCap;           // engine cap in the same unit
        float m_fSaturate;      // multiplier at which every non-zero value has reached the cap
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
    static void Snapshot(SNAPSHOT& rSnapshot);
    static void Restore(const SNAPSHOT& rSnapshot);
    static void SetCustomValue(SETTING setting, float value, GAMETYPES::DIFFICULTY difficulty);
    static UNIT GetUnit(SETTING setting);
    static bool GetLiveInfo(SETTING setting, LIVEINFO& rInfo);
    static float GetEffectiveMaximum(SETTING setting);
    static void StepValue(SETTING setting, int32 direction, int32 repeat, GAMETYPES::DIFFICULTY difficulty);

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
