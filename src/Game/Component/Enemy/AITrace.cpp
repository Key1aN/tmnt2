#include "AITrace.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "Enemy.hpp"
#include "EnemyCharacter.hpp"
#include "EnemyParameter.hpp"
#include "CharacterCompositor.hpp"
#include "ConcreteAIModerator/BaseGeneralEnemyAI.hpp"

#include "Game/Component/CombatLab/CombatLab.hpp"
#include "Game/Component/GameData/GameData.hpp"
#include "Game/Component/GameMain/DebugDifficulty.hpp"
#include "Game/Component/GameMain/GamePlayer.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/GameStage.hpp"
#include "Game/Component/Player/PlayerCharacter.hpp"
#include "Game/System/GameObject/GameObject.hpp"
#include "Game/System/GameObject/GameObjectManager.hpp"

#ifdef GetObject
#undef GetObject
#endif /* GetObject */

#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>


/*static*/ bool CEnemyAITrace::m_bEnabled = false;


namespace
{
    static CEnemyAITrace::SLOT s_aSlot[CEnemyAITrace::SLOT_MAX];
    static CEnemyAITrace::OVERVIEW s_overview;
    static float s_fLastTime = 0.0f;
    static float s_fLastUpdate = -1.0f;
    static float s_fPauseTime = 0.0f;
    static bool s_bPaused = false;
    static CGameStage* s_pStage = nullptr;

    static FILE* s_pFile = nullptr;
    static bool s_bFileRecording = false;
    static float s_fLastPosSample = -1000.0f;
    static float s_fLastFlush = 0.0f;

    static const float POS_SAMPLE_SEC = 0.1f;
    static const float FLUSH_SEC = 2.0f;


    static const char* s_apszFrequencyName[] =
    {
        "Attack A", "Attack AA", "Attack AAA", "Attack B", "Attack C", "Run attack",
        "Guard vs A-chain", "Guard vs B", "Guard vs shuriken", "Guard vs jump attack",
        "Guard vs run attack", "Guard vs throw",
        "Move approach", "Move formation", "Trigger attack", "Trigger move", "Trigger escape",
        "Trigger low HP", "Fire convergence", "Fire range", "Fire range rate", "Fire repeat",
        "Fire repeat count", "Fire repeat chase", "Attack interval", "Common 1", "Jump attack (common 2)",
    };

    static_assert(COUNT_OF(s_apszFrequencyName) == CEnemyParameter::FREQUENCY_MAX, "update me");


    static float SafeNow(void)
    {
        if (CGameStage::GetCurrent())
            s_fLastTime = CGameProperty::GetTotalElapsedTime();
        return s_fLastTime;
    };


    static CEnemy* LiveEnemy(uint32 hEnemy)
    {
        CGameObject* pObject = CGameObjectManager::GetObject(hEnemy);
        if (!pObject || (pObject->GetType() != GAMEOBJECTTYPE::ENEMY))
            return nullptr;
        return static_cast<CEnemy*>(pObject);
    };


    static void ResetSlot(CEnemyAITrace::SLOT& rSlot)
    {
        std::memset(&rSlot, 0x00, sizeof(rSlot));
        rSlot.m_target = -1;
        rSlot.m_status = -1;
    };


    static int32 FreeLabel(void)
    {
        for (int32 label = 1; label <= CEnemyAITrace::SLOT_MAX; ++label)
        {
            bool bTaken = false;
            for (int32 i = 0; i < CEnemyAITrace::SLOT_MAX; ++i)
            {
                if (s_aSlot[i].m_bUsed && (s_aSlot[i].m_iLabel == label))
                {
                    bTaken = true;
                    break;
                };
            };

            if (!bTaken)
                return label;
        };

        return 0;
    };


    static CEnemyAITrace::SLOT* SlotByHandle(uint32 hEnemy, ENEMYID::VALUE id, bool bCreate)
    {
        for (int32 i = 0; i < CEnemyAITrace::SLOT_MAX; ++i)
        {
            if (s_aSlot[i].m_bUsed && (s_aSlot[i].m_hEnemy == hEnemy))
                return &s_aSlot[i];
        };

        if (!bCreate)
            return nullptr;

        for (int32 pass = 0; pass < 2; ++pass)
        {
            for (int32 i = 0; i < CEnemyAITrace::SLOT_MAX; ++i)
            {
                CEnemyAITrace::SLOT& rSlot = s_aSlot[i];
                // second pass reclaims slots whose enemy is already gone
                if (rSlot.m_bUsed && ((pass == 0) || LiveEnemy(rSlot.m_hEnemy)))
                    continue;

                ResetSlot(rSlot);
                rSlot.m_bUsed = true;
                rSlot.m_hEnemy = hEnemy;
                rSlot.m_id = id;
                rSlot.m_iLabel = FreeLabel();
                rSlot.m_fStatusStart = SafeNow();
                rSlot.m_fLastDecisionTime = rSlot.m_fStatusStart;
                return &rSlot;
            };
        };

        return nullptr;
    };


    static int32 OrderTypeId(const CAIThinkOrder& rOrder)
    {
        CAIThinkOrder& rMutable = const_cast<CAIThinkOrder&>(rOrder);
        switch (rOrder.GetOrder())
        {
        case CAIThinkOrder::ORDER_MOVE:   return AIOT::Get(rMutable.OrderMove().m_iMoveType);
        case CAIThinkOrder::ORDER_ATTACK: return AIOT::Get(rMutable.OrderAttack().m_iAttackType);
        case CAIThinkOrder::ORDER_WAIT:   return AIOT::Get(rMutable.OrderWait().m_iWaitType);
        default:                          return 0;
        };
    };


    static int32 OrderTarget(const CAIThinkOrder& rOrder)
    {
        CAIThinkOrder& rMutable = const_cast<CAIThinkOrder&>(rOrder);
        switch (rOrder.GetOrder())
        {
        case CAIThinkOrder::ORDER_MOVE:
            return (AIOT::GetTarget(rMutable.OrderMove().m_iMoveType) ? rMutable.OrderMove().m_iPlayerNo : -1);
        case CAIThinkOrder::ORDER_ATTACK:
            return (AIOT::GetTarget(rMutable.OrderAttack().m_iAttackType) ? rMutable.OrderAttack().m_iPlayerNo : -1);
        case CAIThinkOrder::ORDER_WAIT:
            return (AIOT::GetTarget(rMutable.OrderWait().m_iWaitType) ? rMutable.OrderWait().m_iPlayerNo : -1);
        default:
            return -1;
        };
    };


    static int32 BaseFrequency(const CEnemyCharacter* pChr, int32 index)
    {
        const uint8* pBase = nullptr;
        const uint8* pNow = nullptr;
        int32 count = 0;
        if (!pChr || !pChr->GetDebugFrequencyRows(&pBase, &pNow, &count) || (index < 0) || (index >= count))
            return -1;
        return static_cast<int32>(pBase[index]);
    };
};


/*static*/ void CEnemyAITrace::SetEnabled(bool bEnable)
{
    if (m_bEnabled == bEnable)
        return;

    m_bEnabled = bEnable;
    if (m_bEnabled)
        Clear();
    else
        SetFileRecording(false);
};


/*static*/ void CEnemyAITrace::SetFileRecording(bool bEnable)
{
    if (bEnable == s_bFileRecording)
        return;

    if (bEnable)
    {
        if (!m_bEnabled)
            SetEnabled(true);

        s_pFile = std::fopen("TMNT2_aitrace.jsonl", "ab");
        s_bFileRecording = (s_pFile != nullptr);
        if (!s_bFileRecording)
            return;

        GAMETYPES::DIFFICULTY optionDifficulty = CGameData::Option().Play().GetDifficulty();
        char multipliers[512];
        multipliers[0] = '\0';
        for (int32 i = 0; i < CDebugDifficulty::SETTING_NUM; ++i)
        {
            char item[32];
            std::snprintf(item, sizeof(item), "%s%.2f", (i ? "," : ""),
                          CDebugDifficulty::GetValue(static_cast<CDebugDifficulty::SETTING>(i), optionDifficulty));
            strcat_s(multipliers, sizeof(multipliers), item);
        };

        writeLine("{\"k\":\"hdr\",\"v\":1,\"t\":%.3f,\"preset\":\"%s\",\"override\":%d,\"mult\":[%s],"
                  "\"lab\":%d,\"ruleset\":\"%s\",\"ai\":\"%s\","
                  "\"mult_names\":[\"enemy_damage\",\"enemy_hp\",\"player_damage\",\"attack_freq\",\"guard_freq\","
                  "\"projectile_freq\",\"projectile_range\",\"ai_thinking\",\"ai_activity\",\"ai_awareness\","
                  "\"attack_interval\",\"knockback\"]}",
                  SafeNow(),
                  CDebugDifficulty::GetPresetName(),
                  CDebugDifficulty::IsOverrideEnabled() ? 1 : 0,
                  multipliers,
                  CCombatLab::IsActive() ? 1 : 0,
                  CCombatLab::GetRulesetName(CCombatLab::GetRuleset()),
                  CCombatLab::GetAIModeName(CCombatLab::GetAIMode()));
        s_fLastPosSample = -1000.0f;
    }
    else
    {
        if (s_pFile)
        {
            WriteSummary("recording stopped");
            std::fclose(s_pFile);
            s_pFile = nullptr;
        };
        s_bFileRecording = false;
    };
};


/*static*/ bool CEnemyAITrace::IsFileRecording(void)
{
    return s_bFileRecording;
};


/*static*/ void CEnemyAITrace::SetTimelinePaused(bool bPause)
{
    if (bPause && !s_bPaused)
        s_fPauseTime = SafeNow();
    s_bPaused = bPause;
};


/*static*/ bool CEnemyAITrace::IsTimelinePaused(void)
{
    return s_bPaused;
};


/*static*/ void CEnemyAITrace::Clear(void)
{
    for (int32 i = 0; i < SLOT_MAX; ++i)
        ResetSlot(s_aSlot[i]);
    std::memset(&s_overview, 0x00, sizeof(s_overview));
    s_fLastUpdate = -1.0f;
    s_bPaused = false;
};


/*static*/ void CEnemyAITrace::OnStageChanged(void)
{
    // Enemy handles are only meaningful inside one stage.
    for (int32 i = 0; i < SLOT_MAX; ++i)
        ResetSlot(s_aSlot[i]);
    s_fLastUpdate = -1.0f;
};


/*static*/ float CEnemyAITrace::Now(void)
{
    return SafeNow();
};


/*static*/ float CEnemyAITrace::TimelineNow(void)
{
    return (s_bPaused ? s_fPauseTime : SafeNow());
};


/*static*/ void CEnemyAITrace::Update(void)
{
    if (!m_bEnabled)
        return;

    CGameStage* pStage = CGameStage::GetCurrent();
    if (pStage != s_pStage)
    {
        s_pStage = pStage;
        OnStageChanged();
    };

    if (!pStage)
        return;

    float now = SafeNow();
    float dt = (s_fLastUpdate < 0.0f ? 0.0f : now - s_fLastUpdate);
    if ((dt < 0.0f) || (dt > 0.5f))
        dt = 0.0f;  // stage restart or long hitch: do not count it
    s_fLastUpdate = now;

    // release slots of enemies that no longer exist
    for (int32 i = 0; i < SLOT_MAX; ++i)
    {
        if (s_aSlot[i].m_bUsed && !LiveEnemy(s_aSlot[i].m_hEnemy))
        {
            if (s_bFileRecording)
                writeLine("{\"k\":\"gone\",\"t\":%.3f,\"e\":%d}", now, s_aSlot[i].m_iLabel);
            ResetSlot(s_aSlot[i]);
        };
    };

    OVERVIEW& ov = s_overview;
    ov.m_nActive = 0;
    ov.m_nInRange = 0;
    ov.m_nAttacking = 0;
    std::memset(ov.m_anCategory, 0x00, sizeof(ov.m_anCategory));

    int32 nAttackingBefore = 0;
    for (int32 i = 0; i < SLOT_MAX; ++i)
    {
        if (s_aSlot[i].m_bUsed && (s_aSlot[i].m_category == CAT_ATTACK) && (s_aSlot[i].m_status >= 0))
            ++nAttackingBefore;
    };

    bool bPosSample = (s_bFileRecording && ((now - s_fLastPosSample) >= POS_SAMPLE_SEC));
    char posLine[1800];
    int32 posLen = 0;
    if (bPosSample)
    {
        s_fLastPosSample = now;
        RwV3d vPlayer = Math::VECTOR3_ZERO;
        float fPlayerDir = 0.0f;
        int32 playerStatus = -1;
        IGamePlayer* pPlayer = (CGameProperty::GetPlayerNum() > 0 ? CGameProperty::Player(0) : nullptr);
        if (pPlayer && pPlayer->IsAlive())
        {
            pPlayer->GetPosition(&vPlayer);
            fPlayerDir = pPlayer->GetRotY();
            CPlayerCharacter* pChr = pPlayer->GetCurrentCharacter();
            if (pChr)
                playerStatus = static_cast<int32>(pChr->GetStatus());
        };
        posLen = std::snprintf(posLine, sizeof(posLine), "{\"k\":\"pos\",\"t\":%.3f,\"p\":[%.2f,%.2f,%.3f,%d],\"e\":[",
                               now, vPlayer.x, vPlayer.z, fPlayerDir, playerStatus);
    };

    int32 nPosEnemy = 0;
    int32 enemyMax = CGameProperty::GetEnemyMax();
    for (int32 i = 0; i < enemyMax; ++i)
    {
        CEnemy* pEnemy = CGameProperty::GetEnemy(i);
        if (!pEnemy)
            continue;

        SLOT* pSlot = SlotByHandle(pEnemy->GetHandle(), pEnemy->GetID(), true);
        if (!pSlot)
            continue;

        CEnemyCharacter& rChr = pEnemy->Character();
        int32 status = static_cast<int32>(rChr.GetStatus());
        uint8 category = static_cast<uint8>(CategoryOf(status));

        if (status != pSlot->m_status)
        {
            bool bAttackStart = ((category == CAT_ATTACK) && (pSlot->m_category != CAT_ATTACK || pSlot->m_status < 0));
            if (bAttackStart)
            {
                ++pSlot->m_nAttackStarts;
                ++ov.m_nAttackStarts;
                if (nAttackingBefore > 0)
                    ++ov.m_nAttackStartsOverlapping;
                ++nAttackingBefore;
            };

            if (!s_bPaused || (pSlot->m_nSegment == 0))
            {
                SEGMENT& rSeg = pSlot->m_aSegment[pSlot->m_nSegment % SEGMENT_HISTORY];
                rSeg.m_fStart = now;
                rSeg.m_category = category;
                rSeg.m_status = static_cast<uint8>(status & 0xFF);
                ++pSlot->m_nSegment;
            };

            if (s_bFileRecording)
            {
                writeLine("{\"k\":\"st\",\"t\":%.3f,\"e\":%d,\"id\":%d,\"s\":%d,\"c\":%d,\"tg\":%d,\"d\":%.2f}",
                          now, pSlot->m_iLabel, static_cast<int32>(pSlot->m_id), status, category,
                          pSlot->m_target, pSlot->m_fTargetDist);
            };

            pSlot->m_status = status;
            pSlot->m_category = category;
            pSlot->m_fStatusStart = now;
        };

        pSlot->m_afCategoryTime[category] += dt;

        // Same distance / facing test as CBaseGrapplerAI::IsTakeAttack, without
        // the ground (movable type) test: "close and facing", not a decision.
        pSlot->m_bInRange = false;
        if (pSlot->m_bGeneralAI && (pSlot->m_target >= 0))
        {
            const ENEMYTYPES::CHARACTERISTIC& rAI = rChr.AICharacteristic();
            float fDiff = std::fabs(pSlot->m_fTargetAngle - rChr.Compositor().GetDirection());
            pSlot->m_bInRange = ((pSlot->m_fTargetDist <= rAI.m_fDistanceOfSuitable) &&
                                 (fDiff <= (rAI.m_fRatioOfFrontView * MATH_DEG2RAD(36.0f))));
        };

        ++ov.m_nActive;
        ++ov.m_anCategory[category];
        if (category == CAT_ATTACK)
            ++ov.m_nAttacking;
        if (pSlot->m_bInRange)
            ++ov.m_nInRange;

        if (bPosSample && (posLen > 0) && (posLen < static_cast<int32>(sizeof(posLine)) - 96))
        {
            RwV3d vPos = Math::VECTOR3_ZERO;
            pEnemy->GetPosition(&vPos);
            posLen += std::snprintf(posLine + posLen, sizeof(posLine) - posLen, "%s[%d,%.2f,%.2f,%.3f,%d,%d]",
                                    (nPosEnemy ? "," : ""), pSlot->m_iLabel, vPos.x, vPos.z,
                                    rChr.Compositor().GetDirection(), status, pSlot->m_bInRange ? 1 : 0);
            ++nPosEnemy;
        };
    };

    if (ov.m_nAttacking > ov.m_nMaxAttacking)
        ov.m_nMaxAttacking = ov.m_nAttacking;
    ov.m_fSpan += dt;
    ov.m_afAttackingTime[ov.m_nAttacking < SLOT_MAX ? ov.m_nAttacking : SLOT_MAX] += dt;

    if (bPosSample && (posLen > 0) && (posLen < static_cast<int32>(sizeof(posLine)) - 4))
    {
        std::snprintf(posLine + posLen, sizeof(posLine) - posLen, "]}");
        writeLine("%s", posLine);
    };

    if (s_pFile && ((now - s_fLastFlush) >= FLUSH_SEC))
    {
        std::fflush(s_pFile);
        s_fLastFlush = now;
    };
};


/*static*/ CEnemyAITrace::SLOT* CEnemyAITrace::slotFor(const CEnemyCharacter* pChr, bool bCreate)
{
    if (!pChr)
        return nullptr;

    CEnemyCharacter* pMutable = const_cast<CEnemyCharacter*>(pChr);
    SLOT* pSlot = SlotByHandle(pMutable->GetOwner(), pMutable->GetID(), bCreate);
    if (pSlot)
        pSlot->m_bGeneralAI = true;
    return pSlot;
};


/*static*/ void CEnemyAITrace::RunBegin(const CEnemyCharacter* pChr)
{
    SLOT* pSlot = slotFor(pChr, true);
    if (!pSlot)
        return;

    std::memset(&pSlot->m_pending, 0x00, sizeof(pSlot->m_pending));
    pSlot->m_pending.m_target = -1;
    pSlot->m_bPendingOpen = true;
};


/*static*/ void CEnemyAITrace::RunEnd(const CEnemyCharacter* pChr, float fDist, float fAngle,
                                      float fThinkInterval, float fSinceThink)
{
    SLOT* pSlot = slotFor(pChr, true);
    if (!pSlot)
        return;

    pSlot->m_fTargetDist = fDist;
    pSlot->m_fTargetAngle = fAngle;
    pSlot->m_fThinkInterval = fThinkInterval;
    pSlot->m_fSinceThink = fSinceThink;
    pSlot->m_fLastAIFrame = SafeNow();
    pSlot->m_bPendingOpen = false;
};


/*static*/ void CEnemyAITrace::Event(const CEnemyCharacter* pChr, EVENT kind, int32 idx, float value, float roll, bool bPass)
{
    SLOT* pSlot = slotFor(pChr, true);
    if (!pSlot)
        return;

    // The live target is whatever SearchTarget last produced (read from its
    // own result, so CTargetInfo is never touched by the trace).
    if (kind == EV_TARGET_RESULT)
        pSlot->m_target = static_cast<int16>(idx);

    if (!pSlot->m_bPendingOpen)
        return;

    DECISION& rPending = pSlot->m_pending;
    if (rPending.m_nEvent >= DECISION_EVENT_MAX)
    {
        rPending.m_bEventOverflow = true;
        return;
    };

    EVENTREC& rEvent = rPending.m_aEvent[rPending.m_nEvent++];
    rEvent.m_kind = static_cast<uint8>(kind);
    rEvent.m_pass = (bPass ? 1 : 0);
    rEvent.m_idx = static_cast<int16>(idx);
    rEvent.m_value = value;
    rEvent.m_roll = roll;
};


/*static*/ void CEnemyAITrace::BackAwayTimer(const CEnemyCharacter* pChr, float fCooldown)
{
    SLOT* pSlot = slotFor(pChr, true);
    if (!pSlot)
        return;

    pSlot->m_bHasBackAway = true;
    pSlot->m_fBackAwayCooldown = fCooldown;
};


/*static*/ void CEnemyAITrace::Discard(const CEnemyCharacter* pChr)
{
    SLOT* pSlot = slotFor(pChr, true);
    if (!pSlot || !pSlot->m_bPendingOpen)
        return;

    // A wait/move check that did not produce a new order.
    ++pSlot->m_nWaitChecks;
    const DECISION& rPending = pSlot->m_pending;
    for (int32 i = 0; i < rPending.m_nEvent; ++i)
    {
        const EVENTREC& rEvent = rPending.m_aEvent[i];
        if ((rEvent.m_kind == EV_UNDERWAIT) && !rEvent.m_pass)
            ++pSlot->m_nWaitActivityFail;
        else if ((rEvent.m_kind == EV_TARGET_RESULT) && (rEvent.m_idx < 0))
            ++pSlot->m_nWaitNoTarget;
    };

    pSlot->m_bPendingOpen = false;
};


/*static*/ void CEnemyAITrace::Commit(const CEnemyCharacter* pChr, REASON reason, const CAIThinkOrder& rOrder)
{
    SLOT* pSlot = slotFor(pChr, true);
    if (!pSlot || !pSlot->m_bPendingOpen)
        return;

    float now = SafeNow();
    DECISION& rPending = pSlot->m_pending;
    rPending.m_fTime = now;
    rPending.m_reason = static_cast<uint8>(reason);
    rPending.m_order = static_cast<uint8>(rOrder.GetOrder());
    rPending.m_typeId = static_cast<int16>(OrderTypeId(rOrder));
    rPending.m_target = static_cast<int16>(OrderTarget(rOrder));
    rPending.m_fTargetDist = pSlot->m_fTargetDist;
    rPending.m_nWaitChecks = pSlot->m_nWaitChecks;
    rPending.m_nWaitActivityFail = pSlot->m_nWaitActivityFail;
    rPending.m_nWaitNoTarget = pSlot->m_nWaitNoTarget;
    rPending.m_fPrevOrderSec = now - pSlot->m_fLastDecisionTime;

    pSlot->m_aDecision[pSlot->m_nDecision % DECISION_HISTORY] = rPending;
    ++pSlot->m_nDecision;

    pSlot->m_nWaitChecks = 0;
    pSlot->m_nWaitActivityFail = 0;
    pSlot->m_nWaitNoTarget = 0;
    pSlot->m_fLastDecisionTime = now;
    pSlot->m_bPendingOpen = false;

    ++s_overview.m_nDecisions;
    if (rPending.m_order < CAIThinkOrder::ORDER_MAX)
        ++s_overview.m_anDecisionOrder[rPending.m_order];

    if (s_bFileRecording)
        writeDecision(*pSlot, pSlot->m_aDecision[(pSlot->m_nDecision - 1) % DECISION_HISTORY]);
};


/*static*/ const CEnemyAITrace::SLOT* CEnemyAITrace::GetSlot(int32 index)
{
    if ((index < 0) || (index >= SLOT_MAX) || !s_aSlot[index].m_bUsed)
        return nullptr;
    return &s_aSlot[index];
};


/*static*/ const CEnemyAITrace::SLOT* CEnemyAITrace::FindSlot(uint32 hEnemy)
{
    for (int32 i = 0; i < SLOT_MAX; ++i)
    {
        if (s_aSlot[i].m_bUsed && (s_aSlot[i].m_hEnemy == hEnemy))
            return &s_aSlot[i];
    };
    return nullptr;
};


/*static*/ const CEnemyAITrace::OVERVIEW& CEnemyAITrace::Overview(void)
{
    return s_overview;
};


/*static*/ const CEnemyAITrace::DECISION* CEnemyAITrace::GetDecision(const SLOT& rSlot, int32 ago)
{
    if ((ago < 0) || (ago >= DECISION_HISTORY) || (ago >= rSlot.m_nDecision))
        return nullptr;
    return &rSlot.m_aDecision[(rSlot.m_nDecision - 1 - ago) % DECISION_HISTORY];
};


/*static*/ CEnemyAITrace::CATEGORY CEnemyAITrace::CategoryOf(int32 status)
{
    switch (status)
    {
    case ENEMYTYPES::STATUS_IDLE:
    case ENEMYTYPES::STATUS_THINKING:
        return CAT_WAIT;

    case ENEMYTYPES::STATUS_TURN:
    case ENEMYTYPES::STATUS_WALK:
    case ENEMYTYPES::STATUS_WALK_RIGHT:
    case ENEMYTYPES::STATUS_WALK_LEFT:
    case ENEMYTYPES::STATUS_WALK_BACK:
    case ENEMYTYPES::STATUS_RUN:
    case ENEMYTYPES::STATUS_DASH:
    case ENEMYTYPES::STATUS_JUMP_READY:
    case ENEMYTYPES::STATUS_JUMP:
    case ENEMYTYPES::STATUS_JUMP_2ND:
    case ENEMYTYPES::STATUS_AERIAL:
    case ENEMYTYPES::STATUS_TOUCHDOWN:
        return CAT_MOVE;

    case ENEMYTYPES::STATUS_ATTACK_A:
    case ENEMYTYPES::STATUS_ATTACK_AA:
    case ENEMYTYPES::STATUS_ATTACK_AAA:
    case ENEMYTYPES::STATUS_ATTACK_B:
    case ENEMYTYPES::STATUS_ATTACK_C:
    case ENEMYTYPES::STATUS_ATTACK_RUN:
    case ENEMYTYPES::STATUS_FIRE_A:
    case ENEMYTYPES::STATUS_FIRE_B:
    case ENEMYTYPES::STATUS_FIRE_C:
    case ENEMYTYPES::STATUS_FIRE_AA:
    case ENEMYTYPES::STATUS_FIRE_BB:
    case ENEMYTYPES::STATUS_FIRE_CC:
    case ENEMYTYPES::STATUS_THROW:
    case ENEMYTYPES::STATUS_JUMP_ATTACK:
        return CAT_ATTACK;

    case ENEMYTYPES::STATUS_GUARD:
        return CAT_GUARD;

    case ENEMYTYPES::STATUS_GETUP:
        return CAT_HIT;

    default:
        break;
    };

    if (status >= ENEMYTYPES::STATUS_SPECIAL)
        return CAT_SPECIAL;

    if ((status >= ENEMYTYPES::STATUS_KNOCK) && (status <= ENEMYTYPES::STATUS_THROWN_BACK))
        return CAT_HIT;

    return CAT_OTHER;
};


/*static*/ const char* CEnemyAITrace::CategoryName(int32 category)
{
    switch (category)
    {
    case CAT_WAIT:    return "Waiting";
    case CAT_MOVE:    return "Moving";
    case CAT_ATTACK:  return "Attacking";
    case CAT_GUARD:   return "Guarding";
    case CAT_SPECIAL: return "Special";
    case CAT_HIT:     return "Hit / down";
    default:          return "Other";
    };
};


/*static*/ void CEnemyAITrace::CategoryColor(int32 category, uint8* rgb)
{
    static const uint8 s_aColor[CAT_NUM][3] =
    {
        { 0x9A, 0xA2, 0xB0 },   // waiting: gray
        { 0x48, 0x9C, 0xFF },   // moving: blue
        { 0xFF, 0x40, 0x40 },   // attacking: red
        { 0xFF, 0xE0, 0x30 },   // guarding: yellow
        { 0xFF, 0x98, 0x28 },   // special (back away): orange
        { 0xB0, 0x60, 0xFF },   // hit / down: purple
        { 0xF0, 0xF0, 0xF0 },   // other: white
    };

    int32 c = ((category >= 0) && (category < CAT_NUM) ? category : CAT_OTHER);
    rgb[0] = s_aColor[c][0];
    rgb[1] = s_aColor[c][1];
    rgb[2] = s_aColor[c][2];
};


/*static*/ const char* CEnemyAITrace::OrderName(int32 order)
{
    switch (order)
    {
    case CAIThinkOrder::ORDER_NOTHING: return "NOTHING";
    case CAIThinkOrder::ORDER_WAIT:    return "WAIT";
    case CAIThinkOrder::ORDER_MOVE:    return "MOVE";
    case CAIThinkOrder::ORDER_RUN:     return "RUN HOME";
    case CAIThinkOrder::ORDER_ATTACK:  return "ATTACK";
    case CAIThinkOrder::ORDER_DEFENCE: return "GUARD";
    default:                           return "?";
    };
};


/*static*/ const char* CEnemyAITrace::TypeName(int32 order, int32 typeId)
{
    if ((order != CAIThinkOrder::ORDER_MOVE) && (order != CAIThinkOrder::ORDER_ATTACK))
        return "";

    switch (typeId)
    {
    case AIOT::MoveTurn:      return "turn";
    case AIOT::MoveWalkLeft:  return "side-walk left";
    case AIOT::MoveWalkRight: return "side-walk right";
    case AIOT::MoveWalk:      return "walk";
    case AIOT::MoveRun:       return "run";
    case AIOT::MoveJump:      return "jump";
    case AIOT::AttackA:       return "A";
    case AIOT::AttackAA:      return "AA";
    case AIOT::AttackAAA:     return "AAA";
    case AIOT::AttackB:       return "B";
    case AIOT::AttackC:       return "C";
    case AIOT::AttackRun:     return "run attack";
    case AIOT::FireA:         return "fire A";
    case AIOT::FireB:         return "fire B";
    case AIOT::FireC:         return "fire C";
    case AIOT::FireRepeatA:   return "fire AA";
    case AIOT::FireRepeatB:   return "fire BB";
    case AIOT::FireRepeatC:   return "fire CC";
    case AIOT::Special1:      return "back away";
    default:                  return "";
    };
};


/*static*/ const char* CEnemyAITrace::ReasonName(int32 reason)
{
    switch (reason)
    {
    case REASON_ACCEPT:    return "previous order done";
    case REASON_REFUSE:    return "previous order refused / timed out";
    case REASON_THINKOVER: return "re-decided while waiting/moving";
    case REASON_FORCED:    return "forced reaction while waiting/moving";
    default:               return "?";
    };
};


/*static*/ const char* CEnemyAITrace::FrequencyName(int32 index)
{
    if ((index < 0) || (index >= COUNT_OF(s_apszFrequencyName)))
        return "?";
    return s_apszFrequencyName[index];
};


/*static*/ void CEnemyAITrace::FormatEvent(const EVENTREC& rEvent, const CEnemyCharacter* pChr, char* buffer, size_t capacity)
{
    int32 pct = static_cast<int32>((rEvent.m_value * 100.0f) + 0.5f);
    int32 roll = static_cast<int32>(rEvent.m_roll * 100.0f);
    const char* pszPass = (rEvent.m_pass ? "YES" : "no");

    switch (rEvent.m_kind)
    {
    case EV_TARGET:
        if (rEvent.m_idx < 0)
            std::snprintf(buffer, capacity, "Target: no player passes view / distance / path checks");
        else
            std::snprintf(buffer, capacity, "Take P%d as target? activity %d%%, roll %d -> %s",
                          rEvent.m_idx + 1, pct, roll, pszPass);
        break;

    case EV_TARGET_KEEP:
        std::snprintf(buffer, capacity, "Switch target? impulsiveness %d%%, roll %d -> %s", pct, roll, pszPass);
        break;

    case EV_TARGET_RESULT:
        if (rEvent.m_idx < 0)
            std::snprintf(buffer, capacity, "Target result: NONE");
        else
            std::snprintf(buffer, capacity, "Target result: P%d", rEvent.m_idx + 1);
        break;

    case EV_PATROL_RUN:
        std::snprintf(buffer, capacity, "Outside patrol area: run home? move %d%%, roll %d -> %s", pct, roll, pszPass);
        break;

    case EV_ELIGIBLE:
        {
            int32 f = rEvent.m_idx;
            std::snprintf(buffer, capacity, "Checks: %s%s%s%s%s%s%s",
                          (f & ELIGIBLE_NO_TARGET) ? "no target  " : "",
                          (f & ELIGIBLE_BACKAWAY) ? "BACK-AWAY  " : "",
                          (f & ELIGIBLE_JUMP_ATTACK) ? "JUMP-ATTACK  " : "",
                          (f & ELIGIBLE_GUARD) ? "player attacking in front (guard)  " : "",
                          (f & ELIGIBLE_ATTACK) ? "in attack range  " : "",
                          (f & ELIGIBLE_RUN_ATTACK) ? "in run-attack range  " : "",
                          (f & ELIGIBLE_MOVE_REFUSED) ? "last move refused" :
                          ((f & (ELIGIBLE_NO_TARGET | ELIGIBLE_GUARD | ELIGIBLE_ATTACK | ELIGIBLE_RUN_ATTACK |
                                 ELIGIBLE_BACKAWAY | ELIGIBLE_JUMP_ATTACK)) ? "" : "target out of range"));
        }
        break;

    case EV_SELECT:
        std::snprintf(buffer, capacity, "Action selector -> %s", OrderName(rEvent.m_idx));
        break;

    case EV_FREQ:
        {
            int32 base = BaseFrequency(pChr, rEvent.m_idx);
            int32 freq = static_cast<int32>(rEvent.m_value + 0.5f);
            if ((base >= 0) && (base != freq))
                std::snprintf(buffer, capacity, "%s: %d%% (retail %d%%), roll %d -> %s",
                              FrequencyName(rEvent.m_idx), freq, base, static_cast<int32>(rEvent.m_roll), pszPass);
            else
                std::snprintf(buffer, capacity, "%s: %d%%, roll %d -> %s",
                              FrequencyName(rEvent.m_idx), freq, static_cast<int32>(rEvent.m_roll), pszPass);
        }
        break;

    case EV_FALLBACK:
        std::snprintf(buffer, capacity, "%s not taken -> falls back to WAIT", OrderName(rEvent.m_idx));
        break;

    case EV_WAIT_MOVE:
        std::snprintf(buffer, capacity, "Wait: wander to random point? move %d%%, roll %d -> %s", pct, roll, pszPass);
        break;

    case EV_WAIT_ACTIVITY:
        std::snprintf(buffer, capacity, "Wait: turn toward target instead of standing? activity %d%%, roll %d -> %s",
                      pct, roll, pszPass);
        break;

    case EV_WAIT_NOPOINT:
        std::snprintf(buffer, capacity, "Wait: random wander point not reachable");
        break;

    case EV_MOVE_ROLL:
        std::snprintf(buffer, capacity, "Move: approach target? move %d%%, roll %d -> %s", pct, roll, pszPass);
        break;

    case EV_MOVE_ACTIVITY:
        std::snprintf(buffer, capacity, "Move (close): run instead of walk? activity %d%%, roll %d -> %s",
                      pct, roll, pszPass);
        break;

    case EV_MOVE_FAIL:
        {
            static const char* s_apszReason[] = { "no active player", "no path to player", "player beyond action radius", "already facing (no turn needed)" };
            int32 r = Clamp(static_cast<int32>(rEvent.m_idx), 0, COUNT_OF(s_apszReason) - 1);
            std::snprintf(buffer, capacity, "Move refused: %s", s_apszReason[r]);
        }
        break;

    case EV_UNDERWAIT:
        std::snprintf(buffer, capacity, "Stop waiting and re-decide? activity %d%%, roll %d -> %s", pct, roll, pszPass);
        break;

    case EV_GUARD_REACT:
        std::snprintf(buffer, capacity, "Player attacking in front while waiting/moving -> guard roll");
        break;

    case EV_BACKAWAY:
        std::snprintf(buffer, capacity, "Back-away: player attacking in front, cooldown ready -> back away (no roll)");
        break;

    case EV_ATTACK_BLOCKED:
        {
            static const char* s_apszBlocked[] = { "no active player", "player in an unhittable state", "player is down" };
            int32 r = Clamp(static_cast<int32>(rEvent.m_idx), 0, COUNT_OF(s_apszBlocked) - 1);
            std::snprintf(buffer, capacity, "Attack blocked: %s", s_apszBlocked[r]);
        }
        break;

    default:
        std::snprintf(buffer, capacity, "event %d", rEvent.m_kind);
        break;
    };
};


/*static*/ void CEnemyAITrace::writeLine(const char* pszFormat, ...)
{
    if (!s_pFile)
        return;

    char line[2048];
    va_list args;
    va_start(args, pszFormat);
    int32 n = std::vsnprintf(line, sizeof(line) - 2, pszFormat, args);
    va_end(args);

    if (n < 0)
        return;
    if (n > static_cast<int32>(sizeof(line)) - 2)
        n = static_cast<int32>(sizeof(line)) - 2;

    line[n++] = '\n';
    std::fwrite(line, 1, static_cast<size_t>(n), s_pFile);
};


/*static*/ void CEnemyAITrace::writeDecision(const SLOT& rSlot, const DECISION& rDecision)
{
    char events[1500];
    int32 len = 0;
    events[0] = '\0';
    for (int32 i = 0; i < rDecision.m_nEvent; ++i)
    {
        const EVENTREC& rEvent = rDecision.m_aEvent[i];
        int32 written = std::snprintf(events + len, sizeof(events) - len, "%s[%d,%d,%.3f,%.3f,%d]",
                                      (i ? "," : ""), rEvent.m_kind, rEvent.m_idx, rEvent.m_value,
                                      rEvent.m_roll, rEvent.m_pass);
        if ((written < 0) || (len + written >= static_cast<int32>(sizeof(events))))
            break;
        len += written;
    };

    writeLine("{\"k\":\"dec\",\"t\":%.3f,\"e\":%d,\"id\":%d,\"r\":%d,\"o\":%d,\"ty\":%d,\"tg\":%d,\"d\":%.2f,"
              "\"wc\":%d,\"wa\":%d,\"wn\":%d,\"prev\":%.3f,\"ev\":[%s]}",
              rDecision.m_fTime, rSlot.m_iLabel, static_cast<int32>(rSlot.m_id), rDecision.m_reason,
              rDecision.m_order, rDecision.m_typeId, rDecision.m_target, rDecision.m_fTargetDist,
              rDecision.m_nWaitChecks, rDecision.m_nWaitActivityFail, rDecision.m_nWaitNoTarget,
              rDecision.m_fPrevOrderSec, events);
};


/*static*/ void CEnemyAITrace::WriteSummary(const char* pszReason)
{
    if (!s_pFile)
        return;

    const OVERVIEW& ov = s_overview;
    char hist[256];
    hist[0] = '\0';
    for (int32 n = 0; n <= SLOT_MAX; ++n)
    {
        if ((n > ov.m_nMaxAttacking) && (n > 0))
            break;
        char item[24];
        std::snprintf(item, sizeof(item), "%s%.3f", (n ? "," : ""), ov.m_afAttackingTime[n]);
        strcat_s(hist, sizeof(hist), item);
    };

    writeLine("{\"k\":\"sum\",\"t\":%.3f,\"why\":\"%s\",\"span\":%.3f,\"attack_starts\":%d,\"overlapping_starts\":%d,"
              "\"max_attacking\":%d,\"time_by_attacking\":[%s],\"decisions\":%d,\"orders\":[%d,%d,%d,%d,%d,%d]}",
              SafeNow(), pszReason ? pszReason : "", ov.m_fSpan, ov.m_nAttackStarts, ov.m_nAttackStartsOverlapping,
              ov.m_nMaxAttacking, hist, ov.m_nDecisions,
              ov.m_anDecisionOrder[0], ov.m_anDecisionOrder[1], ov.m_anDecisionOrder[2],
              ov.m_anDecisionOrder[3], ov.m_anDecisionOrder[4], ov.m_anDecisionOrder[5]);
    std::fflush(s_pFile);
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
