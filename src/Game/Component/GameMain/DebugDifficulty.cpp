#include "DebugDifficulty.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "ExtendedDifficulty.hpp"
#include "GameProperty.hpp"
#include "GameStage.hpp"

#include "Game/Component/GameData/GameData.hpp"

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
        // Capped settings: the absolute maximum is only the hard bound for a
        // base value of 1. The usable maximum is GetEffectiveMaximum(), the
        // multiplier at which the live enemies' values reach the engine cap.
        { 0.00f, 99.00f, 0.05f }, // attack frequency
        { 0.00f, 99.00f, 0.05f }, // guard frequency
        { 0.00f, 99.00f, 0.05f }, // projectile frequency
        { 0.10f, 25.00f, 0.05f }, // projectile range
        { 0.10f, 20.00f, 0.05f }, // AI thinking
        { 0.10f, 20.00f, 0.05f }, // AI activity
        { 0.10f, 20.00f, 0.05f }, // AI awareness
        { 0.10f,  3.00f, 0.05f }, // attack interval
        { 0.10f,  3.00f, 0.05f }, // knockback threshold
    };


    // Engine caps (see ScaleFrequency and the AI code that consumes them).
    static const float CAP_FREQUENCY_PERCENT = 99.0f;   // chance tables: roll 0..99 < value
    static const float CAP_FIRE_RANGE        = 255.0f;  // uint8 world distance
    static const float CAP_RATE_PERCENT      = 100.0f;  // thinking rate 1.0 = no wait; activity roll 0..1
    static const float CAP_VIEW              = 5.0f;    // 36 deg x view = 180 deg facing cone
    static const float IDLE_MAXIMUM          = 10.0f;   // capped settings when no enemy is alive


    static bool IsCappedSetting(CDebugDifficulty::SETTING setting)
    {
        return (CDebugDifficulty::GetUnit(setting) != CDebugDifficulty::UNIT_MULTIPLIER);
    };


    static float StepAt(CDebugDifficulty::SETTING setting, float value)
    {
        // Bigger steps higher up so large ranges stay quick to cross.
        float step = s_aSettingLimit[setting].step;
        if (value >= 20.0f)
            return step * 20.0f;
        if (value >= 5.0f)
            return step * 10.0f;
        if (value >= 2.0f)
            return step * 2.0f;
        return step;
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
    // No stage means no CGameProperty object and no enemies (Combat Lab
    // changes presets before its stage is created and after it is destroyed).
    if (!CGameStage::GetCurrent())
        return;

    int32 enemyMax = CGameProperty::GetEnemyMax();
    for (int32 i = 0; i < enemyMax; ++i)
    {
        CEnemy* pEnemy = CGameProperty::GetEnemy(i);
        if (pEnemy)
            pEnemy->Character().RefreshDebugDifficultyParameters();
    };
};


/*static*/ void CDebugDifficulty::Snapshot(SNAPSHOT& rSnapshot)
{
    rSnapshot.m_preset = m_preset;
    rSnapshot.m_bOverrideEnabled = m_bOverrideEnabled;
    for (int32 i = 0; i < SETTING_NUM; ++i)
        rSnapshot.m_afValue[i] = m_afValue[i];
};


/*static*/ void CDebugDifficulty::Restore(const SNAPSHOT& rSnapshot)
{
    m_preset = rSnapshot.m_preset;
    m_bOverrideEnabled = rSnapshot.m_bOverrideEnabled;
    for (int32 i = 0; i < SETTING_NUM; ++i)
        m_afValue[i] = ClampSetting(static_cast<SETTING>(i), rSnapshot.m_afValue[i]);

    RefreshLiveEnemies();
};


/*static*/ void CDebugDifficulty::SetCustomValue(
    SETTING setting,
    float value,
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

    m_afValue[setting] = ClampSetting(setting, value);
    m_preset = PRESET_CUSTOM;
    RefreshLiveEnemies();
};


/*static*/ CDebugDifficulty::UNIT CDebugDifficulty::GetUnit(SETTING setting)
{
    switch (setting)
    {
    case SETTING_ATTACK_FREQUENCY:
    case SETTING_GUARD_FREQUENCY:
    case SETTING_PROJECTILE_FREQUENCY:
    case SETTING_AI_THINKING:
    case SETTING_AI_ACTIVITY:
        return UNIT_PERCENT;

    case SETTING_PROJECTILE_RANGE:
        return UNIT_DISTANCE;

    case SETTING_AI_AWARENESS:
        return UNIT_VIEW;

    default:
        return UNIT_MULTIPLIER;
    };
};


/*static*/ bool CDebugDifficulty::GetLiveInfo(SETTING setting, LIVEINFO& rInfo)
{
    rInfo.m_bValid = false;
    rInfo.m_nEnemy = 0;
    rInfo.m_fNowMin = 0.0f;
    rInfo.m_fNowMax = 0.0f;
    rInfo.m_fCap = 0.0f;
    rInfo.m_fSaturate = 0.0f;

    if (!IsCappedSetting(setting) || !CGameStage::GetCurrent())
        return false;

    GAMETYPES::DIFFICULTY optionDifficulty = CGameData::Option().Play().GetDifficulty();

    int32 enemyMax = CGameProperty::GetEnemyMax();
    for (int32 i = 0; i < enemyMax; ++i)
    {
        CEnemy* pEnemy = CGameProperty::GetEnemy(i);
        if (!pEnemy)
            continue;

        const CEnemyCharacter& rChr = pEnemy->Character();
        float baseScale = (rChr.IsDebugBaseIncludingAggression() ? GetBuiltInValue(setting, optionDifficulty) : 1.0f);
        if (baseScale <= 0.0f)
            baseScale = 1.0f;

        // (base, now) pairs this setting drives on this enemy
        float aBase[8];
        float aNow[8];
        int32 nPair = 0;
        float cap = 0.0f;

        if ((setting == SETTING_AI_THINKING) || (setting == SETTING_AI_ACTIVITY) || (setting == SETTING_AI_AWARENESS))
        {
            const ENEMYTYPES::CHARACTERISTIC& rBase = rChr.GetDebugBaseAICharacteristic();
            const ENEMYTYPES::CHARACTERISTIC& rNow = rChr.AICharacteristic();
            if (setting == SETTING_AI_THINKING)
            {
                aBase[0] = rBase.m_fThinkingFrequency * 100.0f;
                aNow[0] = rNow.m_fThinkingFrequency * 100.0f;
                cap = CAP_RATE_PERCENT;
            }
            else if (setting == SETTING_AI_ACTIVITY)
            {
                aBase[0] = rBase.m_fRatioOfActivity * 100.0f;
                aNow[0] = rNow.m_fRatioOfActivity * 100.0f;
                cap = CAP_RATE_PERCENT;
            }
            else
            {
                aBase[0] = rBase.m_fRatioOfFrontView;
                aNow[0] = rNow.m_fRatioOfFrontView;
                cap = CAP_VIEW;
            };
            nPair = 1;
        }
        else
        {
            const uint8* pBaseRow = nullptr;
            const uint8* pNowRow = nullptr;
            int32 count = 0;
            if (!rChr.GetDebugFrequencyRows(&pBaseRow, &pNowRow, &count))
                continue;

            cap = (setting == SETTING_PROJECTILE_RANGE ? CAP_FIRE_RANGE : CAP_FREQUENCY_PERCENT);
            for (int32 index = 0; (index < count) && (nPair < COUNT_OF(aBase)); ++index)
            {
                bool bMine = false;
                switch (setting)
                {
                case SETTING_ATTACK_FREQUENCY:     bMine = IsAttackFrequencyIndex(index); break;
                case SETTING_GUARD_FREQUENCY:      bMine = IsGuardFrequencyIndex(index); break;
                case SETTING_PROJECTILE_FREQUENCY: bMine = IsProjectileFrequencyIndex(index); break;
                case SETTING_PROJECTILE_RANGE:     bMine = (index == 19); break;
                default: break;
                };

                if (!bMine)
                    continue;

                aBase[nPair] = static_cast<float>(pBaseRow[index]);
                aNow[nPair] = static_cast<float>(pNowRow[index]);
                ++nPair;
            };
        };

        bool bCounted = false;
        for (int32 n = 0; n < nPair; ++n)
        {
            if (aBase[n] <= 0.0f)
                continue;   // a base of zero stays zero at any multiplier

            float now = aNow[n];
            if (now > cap)
                now = cap;

            if (!rInfo.m_bValid || (now < rInfo.m_fNowMin))
                rInfo.m_fNowMin = now;
            if (!rInfo.m_bValid || (now > rInfo.m_fNowMax))
                rInfo.m_fNowMax = now;

            float saturate = (cap * baseScale) / aBase[n];
            if (saturate > rInfo.m_fSaturate)
                rInfo.m_fSaturate = saturate;

            rInfo.m_fCap = cap;
            rInfo.m_bValid = true;
            bCounted = true;
        };

        if (bCounted)
            ++rInfo.m_nEnemy;
    };

    return rInfo.m_bValid;
};


/*static*/ float CDebugDifficulty::GetEffectiveMaximum(SETTING setting)
{
    ASSERT(setting >= 0);
    ASSERT(setting < SETTING_NUM);

    float absolute = s_aSettingLimit[setting].maximum;
    if (!IsCappedSetting(setting))
        return absolute;

    LIVEINFO info;
    if (!GetLiveInfo(setting, info))
        return (absolute < IDLE_MAXIMUM ? absolute : IDLE_MAXIMUM);

    // Round up to the 0.05 grid so the cap is really reached.
    float step = s_aSettingLimit[setting].step;
    float maximum = static_cast<float>(static_cast<int32>((info.m_fSaturate / step) + 0.999f)) * step;
    return Clamp(maximum, s_aSettingLimit[setting].minimum, absolute);
};


/*static*/ void CDebugDifficulty::StepValue(
    SETTING setting,
    int32 direction,
    int32 repeat,
    GAMETYPES::DIFFICULTY difficulty
)
{
    ASSERT(setting >= 0);
    ASSERT(setting < SETTING_NUM);

    if ((direction == 0) || (repeat <= 0))
        return;

    if (!m_bOverrideEnabled)
    {
        LoadBuiltInValues(difficulty);
        m_bOverrideEnabled = true;
    };

    float value = m_afValue[setting];
    float minimum = s_aSettingLimit[setting].minimum;

    // Never raise past the point where nothing changes any more, but do not
    // pull an already higher value down just because the enemies changed.
    float maximum = GetEffectiveMaximum(setting);
    if (maximum < value)
        maximum = value;

    for (int32 i = 0; i < repeat; ++i)
    {
        float step = StepAt(setting, (direction > 0 ? value : value - 0.001f));
        float next = value + (step * static_cast<float>(direction));
        next = static_cast<float>(static_cast<int32>((next / step) + (next >= 0.0f ? 0.5f : -0.5f))) * step;
        value = Clamp(next, minimum, maximum);
    };

    m_afValue[setting] = ClampSetting(setting, value);
    m_preset = PRESET_CUSTOM;
    RefreshLiveEnemies();
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
