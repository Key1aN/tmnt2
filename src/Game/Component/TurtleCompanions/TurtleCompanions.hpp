#pragma once

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/Component/Player/PlayerID.hpp"
#include "Game/Component/GameMain/GameTypes.hpp"
#include "Game/Component/GameMain/StageID.hpp"


class CPlayerCharacter;


//
//  Turtle Companions (Debug Tools builds only).
//
//  Single-player story stages: every turtle of the team is on the field at
//  once. The human controls one of them; the others are driven by a small AI
//  that follows, fights near the human, guards, throws shuriken and catches
//  up when left behind. Change Character (L2 / LT by default) hands control
//  to the next turtle instantly, from any state.
//
//  The team is built on the game's own co-op path: each turtle is its own
//  game player (slot 0..3), so enemies, items, gimmicks, the HUD and the
//  camera already know about every turtle. Only the input source changes:
//  the human slot reads the human's pad, the other slots read the AI.
//
//  Balance is kept at single-player values (team HP, boss HP, heal items),
//  the co-op movement leash is off, and the camera frames the human first.
//
//  With the switch OFF, or the Debug Tools mod disabled in the manager, the
//  stage is built exactly as retail. The switch applies when a stage starts.
//
class CTurtleCompanions
{
public:
    struct AICOMMAND
    {
        bool  m_bMove;
        bool  m_bRun;
        float m_fDirection;     // world yaw, same convention as the manipulator
        bool  m_bDash;
        bool  m_bJump;
        int32 m_iAttack;        // 0 none, 1 A, 2 B, 3 C
        bool  m_bCharge;        // B held
        bool  m_bKnife;
        bool  m_bGuard;
        bool  m_bRecover;
        bool  m_bPassive;
    };

public:
    // settings
    static bool IsEnabled(void);
    static void SetEnabled(bool bEnable);
    static bool IsInstantSwitch(void);
    static float GetBadgeTime(void);
    static bool GetBadgeText(char* pszBuffer, size_t capacity, bool& rbOn);

    // stage lifecycle
    static void OnStageInitialize(void);
    static void OnStageTerminate(void);
    static void OnLoadPlayers(STAGEID::VALUE idStage);     // load sequence: extra team members
    static bool AddPlayers(bool bBlink);                    // true when the team was created
    static bool SavePlayData(void);                         // true when it wrote the play context

    // queries for retail code (all false / retail values while inactive)
    static bool IsActive(void);
    static int32 BalancePlayerNum(int32 nRetailPlayerNum);
    static int32 GetHumanPlayerNo(void);
    static bool IsHumanSlot(int32 nPlayerNo);
    static bool IsAISlot(int32 nPlayerNo);
    static int32 GetInputPort(int32 nPlayerNo, int32 nDefaultPort); // -1 = AI drives this slot
    static bool IsCameraTarget(int32 nPlayerNo);
    static int32 ScaleReceivedDamage(const CPlayerCharacter& rPlayerChr, int32 nDamage);

    // per frame
    static void Period(void);
    static void BuildAICommand(CPlayerCharacter& rPlayerChr, AICOMMAND& rCmd);

    // retail team (companions off): switching outside the idle state
    static bool CanRetailSwitch(const CPlayerCharacter& rPlayerChr, bool& rbResult);

    // Debug Tools page
    static int32 GetItemNum(void);
    static bool IsNumericItem(int32 index);
    static const char* GetItemHeader(int32 index);
    static void FormatItem(int32 index, char* pszBuffer, size_t capacity);
    static void GetItemHelp(int32 index, const char*& rpszScope, const char*& rpszDescription);
    static void AdjustItem(int32 index, int32 direction, int32 multiplier);
    static void ResetItem(int32 index);
    static void ResetAll(void);
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
