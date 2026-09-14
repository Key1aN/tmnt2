#include "DebugDifficulty.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "ExtendedDifficulty.hpp"
#include "GameProperty.hpp"

#include "Game/Component/Enemy/Enemy.hpp"
#include "Game/Component/Enemy/EnemyCharacter.hpp"


CDebugDifficulty::PRESET CDebugDifficulty::m_preset = CDebugDifficulty::PRESET_GAME;
bool CDebugDifficulty::m_bOverrideEnabled = false;
float CDebugDifficulty::m_afValue[CDebugDifficulty::SETTING_NUM] = {};


namespace
{
    struct SETTINGLIMIT
    {
        float minimum;
        float maximum;
        float step;
    };

    static const SETTINGLIMIT s_aSettingLimit[CDebugDifficulty::SETTING_NUM] =
    {
        { 0.00f, 10.00f, 0.10f }, // player damage dealt
        { 0.10f, 10.00f, 0.10f }, // enemy HP
        { 0.00f, 10.00f, 0.10f }, // player damage received
        { 0.00f,  3.00f, 0.05f }, // attack frequency
        { 0.00f,  3.00f, 0.05f }, // guard frequency
        { 0.00f,  3.00f, 0.05f }, // projectile frequency
        { 0.10f,  3.00f, 0.05f }, // projectile range
        { 0.10f,  3.00f, 0.05f }, // AI thinking
        { 0.10f,  3.00f, 0.05f }, // AI activity
        { 0.10f,  3.00f, 0.05f }, // AI awareness
        { 0.10f,  3.00f, 0.05f }, // attack interval
        { 0.10f,  3.00f, 0.05f }, // knockback threshold
    };


    static bool IsAttackFrequencyIndex(int32 index)
    {
        return ((index >= 0) && (index <= 5));
    };


    static bool IsGuardFrequencyIndex(int32 index)
    {
        return ((index >= 6) && (index <= 11));
    };


    static bool IsProjectileFrequencyIndex(int32 index)
    {
        return ((index == 18) || (index == 20) || (index == 21));
    };
};


/*static*/ void CDebugDifficulty::Initialize(void)
{
    m_preset = PRESET_GAME;
    m_bOverrideEnabled = false;
    LoadBuiltInValues(GAMETYPES::DIFFICULTY_NORMAL);
};


/*static*/ bool CDebugDifficulty::IsOverrideEnabled(void)
{
    return m_bOverrideEnabled;
};


/*static*/ CDebugDifficulty::PRESET CDebugDifficulty::GetPreset(void)
{
    return m_preset;
};


/*static*/ const char* CDebugDifficulty::GetPresetName(void)
{
    switch (m_preset)
    {
    case PRESET_GAME:       return "Game difficulty";
    case PRESET_VANILLA:    return "Vanilla";
    case PRESET_VERY_HARD:  return "Very Hard";
    case PRESET_EXTREME:    return "Extreme";
    case PRESET_SOULS_LIKE: return "Souls Like";
    case PRESET_CUSTOM:     return "Custom";
    default:                return "Unknown";
    };
};


/*static*/ float CDebugDifficulty::GetValue(SETTING setting, GAMETYPES::DIFFICULTY difficulty)
{
    ASSERT(setting >= 0);
    ASSERT(setting < SETTING_NUM);

    if (!m_bOverrideEnabled)
        return GetBuiltInValue(setting, difficulty);

    return m_afValue[setting];
};


/*static*/ float CDebugDifficulty::GetBuiltInValue(
    SETTING setting,
    GAMETYPES::DIFFICULTY difficulty
)
{
    switch (setting)
    {
    case SETTING_PLAYER_DAMAGE_DEALT:
        return 1.00f;

    case SETTING_ENEMY_HP:
        return EXTENDEDDIFFICULTY::GetBaseEnemyHPScale(difficulty);

    case SETTING_PLAYER_DAMAGE_RECEIVED:
        return EXTENDEDDIFFICULTY::GetBasePlayerDamageReceivedScale(difficulty);

    case SETTING_ATTACK_FREQUENCY:
    case SETTING_GUARD_FREQUENCY:
    case SETTING_PROJECTILE_FREQUENCY:
    case SETTING_AI_THINKING:
    case SETTING_AI_ACTIVITY:
    case SETTING_AI_AWARENESS:
        return EXTENDEDDIFFICULTY::GetBaseEPBAggressionScale(difficulty);

    case SETTING_PROJECTILE_RANGE:
        return 1.00f;

    case SETTING_ATTACK_INTERVAL:
        return EXTENDEDDIFFICULTY::GetBaseAttackIntervalScale(difficulty);

    case SETTING_KNOCKBACK_THRESHOLD:
        return EXTENDEDDIFFICULTY::GetBaseKnockBackThresholdScale(difficulty);

    default:
        ASSERT(false);
        return 1.00f;
    };
};


/*static*/ float CDebugDifficulty::GetMinimum(SETTING setting)
{
    ASSERT(setting >= 0);
    ASSERT(setting < SETTING_NUM);
    return s_aSettingLimit[setting].minimum;
};


/*static*/ float CDebugDifficulty::GetMaximum(SETTING setting)
{
    ASSERT(setting >= 0);
    ASSERT(setting < SETTING_NUM);
    return s_aSettingLimit[setting].maximum;
};


/*static*/ float CDebugDifficulty::GetStep(SETTING setting)
{
    ASSERT(setting >= 0);
    ASSERT(setting < SETTING_NUM);
    return s_aSettingLimit[setting].step;
};


/*static*/ void CDebugDifficulty::SetPreset(PRESET preset, GAMETYPES::DIFFICULTY difficulty)
{
    ASSERT(preset >= PRESET_GAME);
    ASSERT(preset < PRESET_NUM);

    if (preset == PRESET_GAME)
    {
        m_bOverrideEnabled = false;
        m_preset = PRESET_GAME;
        LoadBuiltInValues(difficulty);
    }
    else
    {
        m_bOverrideEnabled = true;
        m_preset = preset;
        LoadPresetValues(preset);
    };

    RefreshLiveEnemies();
};


/*static*/ void CDebugDifficulty::AdjustValue(
    SETTING setting,
    float delta,
    GAMETYPES::DIFFICULTY difficulty
)
{
    ASSERT(setting >= 0);
    ASSERT(setting < SETTING_NUM);

    if (!m_bOverrideEnabled)
    {
        LoadBuiltInValues(difficulty);
        m_bOverrideEnabled = true;
    };

    m_afValue[setting] = ClampSetting(setting, m_afValue[setting] + delta);
    m_preset = PRESET_CUSTOM;
    RefreshLiveEnemies();
};


/*static*/ void CDebugDifficulty::ResetSetting(
    SETTING setting,
    GAMETYPES::DIFFICULTY difficulty
)
{
    ASSERT(setting >= 0);
    ASSERT(setting < SETTING_NUM);

    if (!m_bOverrideEnabled)
        return;

    m_afValue[setting] = GetBuiltInValue(setting, difficulty);
    m_preset = PRESET_CUSTOM;
    RefreshLiveEnemies();
};


/*static*/ void CDebugDifficulty::ResetAll(GAMETYPES::DIFFICULTY difficulty)
{
    SetPreset(PRESET_GAME, difficulty);
};


/*static*/ uint8 CDebugDifficulty::ScaleFrequency(
    int32 index,
    uint8 value,
    bool bBaseIncludesAggression,
    GAMETYPES::DIFFICULTY difficulty
)
{
    if (!m_bOverrideEnabled)
        return value;

    SETTING setting;
    float baseScale = 1.00f;
    int32 maximum = 99;

    if (IsAttackFrequencyIndex(index))
    {
        setting = SETTING_ATTACK_FREQUENCY;
    }
    else if (IsGuardFrequencyIndex(index))
    {
        setting = SETTING_GUARD_FREQUENCY;
    }
    else if (IsProjectileFrequencyIndex(index))
    {
        setting = SETTING_PROJECTILE_FREQUENCY;
    }
    else if (index == 19)
    {
        setting = SETTING_PROJECTILE_RANGE;
        maximum = 255;
    }
    else
    {
        return value;
    };

    if (bBaseIncludesAggression && (setting != SETTING_PROJECTILE_RANGE))
        baseScale = GetBuiltInValue(setting, difficulty);

    if (baseScale <= 0.0f)
        baseScale = 1.0f;

    float scaled = static_cast<float>(value) * (m_afValue[setting] / baseScale);
    scaled = Clamp(scaled, 0.0f, static_cast<float>(maximum));
    return static_cast<uint8>(scaled + 0.5f);
};


/*static*/ void CDebugDifficulty::RefreshLiveEnemies(void)
{
    int32 enemyMax = CGameProperty::GetEnemyMax();
    for (int32 i = 0; i < enemyMax; ++i)
    {
        CEnemy* pEnemy = CGameProperty::GetEnemy(i);
        if (pEnemy)
            pEnemy->Character().RefreshDebugDifficultyParameters();
    };
};


/*static*/ void CDebugDifficulty::LoadBuiltInValues(GAMETYPES::DIFFICULTY difficulty)
{
    for (int32 i = 0; i < SETTING_NUM; ++i)
    {
        SETTING setting = static_cast<SETTING>(i);
        m_afValue[i] = GetBuiltInValue(setting, difficulty);
    };
};


/*static*/ void CDebugDifficulty::LoadPresetValues(PRESET preset)
{
    for (int32 i = 0; i < SETTING_NUM; ++i)
        m_afValue[i] = 1.00f;

    float enemyHP = 1.00f;
    float damageReceived = 1.00f;
    float aggression = 1.00f;
    float attackInterval = 1.00f;
    float knockback = 1.00f;

    switch (preset)
    {
    case PRESET_VANILLA:
        break;

    case PRESET_VERY_HARD:
        enemyHP = 1.50f;
        damageReceived = 2.50f;
        aggression = 1.15f;
        attackInterval = 0.75f;
        knockback = 0.75f;
        break;

    case PRESET_EXTREME:
        enemyHP = 2.00f;
        damageReceived = 4.00f;
        aggression = 1.25f;
        attackInterval = 0.50f;
        knockback = 0.50f;
        break;

    case PRESET_SOULS_LIKE:
        enemyHP = 2.50f;
        damageReceived = 6.00f;
        aggression = 1.35f;
        attackInterval = 0.30f;
        knockback = 0.30f;
        break;

    default:
        ASSERT(false);
        break;
    };

    m_afValue[SETTING_ENEMY_HP] = enemyHP;
    m_afValue[SETTING_PLAYER_DAMAGE_RECEIVED] = damageReceived;
    m_afValue[SETTING_ATTACK_FREQUENCY] = aggression;
    m_afValue[SETTING_GUARD_FREQUENCY] = aggression;
    m_afValue[SETTING_PROJECTILE_FREQUENCY] = aggression;
    m_afValue[SETTING_AI_THINKING] = aggression;
    m_afValue[SETTING_AI_ACTIVITY] = aggression;
    m_afValue[SETTING_AI_AWARENESS] = aggression;
    m_afValue[SETTING_ATTACK_INTERVAL] = attackInterval;
    m_afValue[SETTING_KNOCKBACK_THRESHOLD] = knockback;
};


/*static*/ float CDebugDifficulty::ClampSetting(SETTING setting, float value)
{
    return Clamp(value, GetMinimum(setting), GetMaximum(setting));
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
