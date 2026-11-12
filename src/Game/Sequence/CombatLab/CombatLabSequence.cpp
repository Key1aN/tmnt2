#include "CombatLabSequence.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/Component/CombatLab/CombatLab.hpp"
#include "Game/Component/GameData/GameData.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/StageInfo.hpp"
#include "Game/System/Map/MapCamera.hpp"
#include "Game/System/Misc/ScreenFade.hpp"
#include "Game/System/Sound/GameSound.hpp"
#include "System/PC/PCCrashReporter.hpp"


CPlayCombatLabSeqState::CPlayCombatLabSeqState(void)
: m_step(STEP_PLAY)
{
    ;
};


void CPlayCombatLabSeqState::OnAttach(CStageBaseSequence* pSeq, const void* pParam)
{
    CGameStage& stage = pSeq->Stage();

    // Player start position, arena props and boxes come from the retail set
    // data. G_SET generators stay idle: only CPlayNexusStageSeqState sends
    // them the generator ACTIVATE event, and the lab never does.
    stage.AddStageObjects();
    stage.AddPlayers();
    stage.AddGauge();

    CMapCamera* pMapCamera = stage.GetMapCamera();
    ASSERT(pMapCamera);
    pMapCamera->SetCameraMode(CMapCamera::MODE_AUTOCHANGE);
    pMapCamera->SetPathMode(CGameProperty::GetPlayerNum() > 1 ?
                            CMapCamera::PATHMODE_MULTIPLAYER :
                            CMapCamera::PATHMODE_SINGLEPLAYER);

    CScreenFade::BlackIn();
    m_step = STEP_PLAY;

    CCombatLab::OnPlayStart(&stage);
};


void CPlayCombatLabSeqState::OnDetach(CStageBaseSequence* pSeq)
{
    CCombatLab::OnPlayEnd();
};


bool CPlayCombatLabSeqState::OnMove(CStageBaseSequence* pSeq)
{
    CGameStage& stage = pSeq->Stage();

    switch (m_step)
    {
    case STEP_PLAY:
        {
            if (!stage.IsPaused() && stage.CheckPauseMenu())
                stage.StartPause(CGameStage::PAUSETYPE_MENU, nullptr);

            if (!stage.IsPaused())
                CCombatLab::Period();

            // Any stage result (pause menu quit, Debug Tools stage actions)
            // or an explicit lab exit/reload leaves the lab. There is no
            // victory/defeat presentation in the lab.
            if (CCombatLab::IsExitRequested() || (stage.GetResult() != CGameStage::RESULT_NONE))
            {
                CPCCrashReporter::Breadcrumb("COMBATLAB leaving play result=%d exit_request=%d",
                                             static_cast<int32>(stage.GetResult()),
                                             CCombatLab::IsExitRequested() ? 1 : 0);
                m_step = STEP_FADEOUT;
            };
        }
        break;

    case STEP_FADEOUT:
        {
            CGameSound::FadeOut(CGameSound::FADESPEED_FAST);
            CScreenFade::BlackOut(0.3f);
            m_step = STEP_FADEOUT_WAIT;
        }
        break;

    case STEP_FADEOUT_WAIT:
        {
            if (!CScreenFade::IsFading())
                m_step = STEP_END;
        }
        break;

    default:
        break;
    };

    return (m_step >= STEP_END);
};


//
// *********************************************************************************
//


/*static*/ CProcess* CCombatLabSequence::Instance(void)
{
    return new CCombatLabSequence;
};


CCombatLabSequence::CCombatLabSequence(void)
{
    ;
};


CCombatLabSequence::~CCombatLabSequence(void)
{
    ;
};


bool CCombatLabSequence::OnAttach(const void* pParam)
{
    // Must run before CStageBaseSequence::OnAttach: it snapshots the record
    // before CGameData::OnBeginStage and sets up play params / game mode.
    CCombatLab::OnSequenceAttach();

    CStageBaseSequence::OnAttach(pParam);

    RegisterStateObject(STATE_LOAD, new CLoadStageSeqState(CGameData::PlayParam().GetStage()), true);
    RegisterStateObject(STATE_PLAY, new CPlayCombatLabSeqState, true);
    ChangeState(STATE_LOAD);

    return true;
};


void CCombatLabSequence::OnDetach(void)
{
    CStageBaseSequence::OnDetach();
    CCombatLab::OnSequenceDetach();
};


void CCombatLabSequence::OnStateDetached(STATE state)
{
    switch (state)
    {
    case STATE_LOAD:
        {
            STAGEID::VALUE idStage = CGameData::PlayParam().GetStage();
            CGameSound::PlayBGM(SDCODE_BGM(CStageInfo::GetBgmNo(idStage)));
            ChangeState(STATE_PLAY);
        }
        break;

    case STATE_PLAY:
        ChangeState(STATE_END);
        break;

    default:
        break;
    };
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
