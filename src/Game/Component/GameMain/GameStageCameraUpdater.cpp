#include "GameStageCameraUpdater.hpp"
#include "GameTypes.hpp"
#include "GameProperty.hpp"
#include "GamePlayer.hpp"
#if defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS)
#include "GameStageDebug.hpp"
#endif /* defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS) */
#ifdef _DEBUG
#include "Game/System/Misc/DebugShape.hpp"
#endif /* _DEBUG */

#include "Game/Component/Enemy/CharacterCompositor.hpp"
#include "Game/System/Map/MapCamera.hpp"
#include "Game/System/GameObject/GameObject.hpp"
#include "Game/System/GameObject/GameObjectManager.hpp"


/*
 *  Collects players for enhanced camera framing:
 *      - skips players that are not alive (their position is reported as world origin)
 *      - group center is center of players bounds instead of average, so single player
 *        that is away from others is not pushed out of the screen by the rest of them
 *      - height of the center ignores own jumps of players inside of small window, so
 *        ground and landing spot are not moving on screen while player is in the air
 *      - subtle look-ahead for single player
 */
class CPlayerFrameTracker
{
private:
    struct TRACK
    {
        bool  m_bValid;
        RwV3d m_vPrevPos;
        float m_fAnchorY;
    };

public:
    static CPlayerFrameTracker& Instance(void);

    CPlayerFrameTracker(void);
    int32 Update(CMapCamera::FRAMETARGET* aTarget, RwV3d* pvCenter);

private:
    static bool IsJumpStatus(PLAYERTYPES::STATUS status);
    void Reset(void);

private:
    TRACK m_aTrack[GAMETYPES::PLAYERS_MAX];
    RwV3d m_vLookAhead;
    float m_fLastTime;
};


/*static*/ CPlayerFrameTracker& CPlayerFrameTracker::Instance(void)
{
    static CPlayerFrameTracker s_PlayerFrameTracker;
    return s_PlayerFrameTracker;
};


CPlayerFrameTracker::CPlayerFrameTracker(void)
: m_vLookAhead(Math::VECTOR3_ZERO)
, m_fLastTime(0.0f)
{
    Reset();
};


/*static*/ bool CPlayerFrameTracker::IsJumpStatus(PLAYERTYPES::STATUS status)
{
    switch (status)
    {
    case PLAYERTYPES::STATUS_JUMP_READY:
    case PLAYERTYPES::STATUS_JUMP:
    case PLAYERTYPES::STATUS_JUMP_2ND:
    case PLAYERTYPES::STATUS_JUMP_WALL:
    case PLAYERTYPES::STATUS_JUMP_WALL_FAILURE:
    case PLAYERTYPES::STATUS_AERIAL:
    case PLAYERTYPES::STATUS_AERIAL_MOVE:
    case PLAYERTYPES::STATUS_ATTACK_JUMP:
    case PLAYERTYPES::STATUS_ATTACK_KNIFE_JUMP:
        return true;

    default:
        return false;
    };
};


void CPlayerFrameTracker::Reset(void)
{
    for (int32 i = 0; i < COUNT_OF(m_aTrack); ++i)
    {
        m_aTrack[i].m_bValid   = false;
        m_aTrack[i].m_vPrevPos = Math::VECTOR3_ZERO;
        m_aTrack[i].m_fAnchorY = 0.0f;
    };

    m_vLookAhead = Math::VECTOR3_ZERO;
};


int32 CPlayerFrameTracker::Update(CMapCamera::FRAMETARGET* aTarget, RwV3d* pvCenter)
{
    /* player moved that far during single update was relocated, not moving */
    const float fRelocationDist = 6.0f;

    const CMapCamera::TUNING& rTuning = CMapCamera::m_tuning;

    float dt = Clamp(CGameProperty::GetElapsedTime(), 0.0f, 0.1f);

    /* updater instances are shared between stages - drop the state if it is stale */
    float fNow = CGameProperty::GetTotalElapsedTime();
    if ((fNow < m_fLastTime) || ((fNow - m_fLastTime) > 0.5f))
        Reset();
    m_fLastTime = fNow;

    int32 numTarget = 0;
    bool  bRelocated = false;
    RwV3d vMin = Math::VECTOR3_ZERO;
    RwV3d vMax = Math::VECTOR3_ZERO;
    RwV3d vVelocity = Math::VECTOR3_ZERO;

    int32 playerNum = Min(CGameProperty::GetPlayerNum(), static_cast<int32>(GAMETYPES::PLAYERS_MAX));
    for (int32 i = 0; i < playerNum; ++i)
    {
        TRACK& rTrack = m_aTrack[i];

        IGamePlayer* pGamePlayer = CGameProperty::Player(i);
        if (!pGamePlayer || !pGamePlayer->IsAlive())
        {
            rTrack.m_bValid = false;
            continue;
        };

        RwV3d vPos = Math::VECTOR3_ZERO;
        pGamePlayer->GetPosition(&vPos);

        RwV3d vMove = Math::VECTOR3_ZERO;
        Math::Vec3_Sub(&vMove, &vPos, &rTrack.m_vPrevPos);

        if (!rTrack.m_bValid || (Math::Vec3_Length(&vMove) > fRelocationDist))
        {
            rTrack.m_bValid   = true;
            rTrack.m_fAnchorY = vPos.y;
            bRelocated = true;
        }
        else
        {
            if (IsJumpStatus(pGamePlayer->GetStatus()))
            {
                /* follow down right away to keep landing visible, follow up only out of window */
                if (vPos.y < rTrack.m_fAnchorY)
                    rTrack.m_fAnchorY = vPos.y;
                else if (vPos.y > (rTrack.m_fAnchorY + rTuning.m_fJumpWindow))
                    rTrack.m_fAnchorY = (vPos.y - rTuning.m_fJumpWindow);
            }
            else
            {
                float t = Clamp((1.0f - std::exp(-rTuning.m_fGroundFollowSpeed * dt)), 0.0f, 1.0f);
                rTrack.m_fAnchorY += ((vPos.y - rTrack.m_fAnchorY) * t);
            };

            if (dt > 0.0f)
                Math::Vec3_Scale(&vVelocity, &vMove, (1.0f / dt));
        };

        rTrack.m_vPrevPos = vPos;

        RwV3d vAnchor = { vPos.x, rTrack.m_fAnchorY, vPos.z };
        if (numTarget)
        {
            vMin.x = Min(vMin.x, vAnchor.x);
            vMin.y = Min(vMin.y, vAnchor.y);
            vMin.z = Min(vMin.z, vAnchor.z);

            vMax.x = Max(vMax.x, vAnchor.x);
            vMax.y = Max(vMax.y, vAnchor.y);
            vMax.z = Max(vMax.z, vAnchor.z);
        }
        else
        {
            vMin = vAnchor;
            vMax = vAnchor;
        };

        aTarget[numTarget].m_vPos     = vPos;
        aTarget[numTarget].m_fUp      = rTuning.m_fTargetHalfHeight;
        aTarget[numTarget].m_fDown    = rTuning.m_fTargetHalfHeight;
        aTarget[numTarget].m_bPrimary = true;
        ++numTarget;
    };

    if (!numTarget)
    {
        m_vLookAhead = Math::VECTOR3_ZERO;
        return 0;
    };

    pvCenter->x = ((vMin.x + vMax.x) * 0.5f);
    pvCenter->y = ((vMin.y + vMax.y) * 0.5f);
    pvCenter->z = ((vMin.z + vMax.z) * 0.5f);

    /* look-ahead: single player only, group has no single movement direction */
    RwV3d vLookAhead = Math::VECTOR3_ZERO;
    if ((numTarget == 1) && !bRelocated && (rTuning.m_fLookAheadDist > 0.0f))
    {
        vLookAhead.x = (vVelocity.x * rTuning.m_fLookAheadTime);
        vLookAhead.z = (vVelocity.z * rTuning.m_fLookAheadTime);

        float fLen = Math::Vec3_Length(&vLookAhead);
        if (fLen > rTuning.m_fLookAheadDist)
            Math::Vec3_Scale(&vLookAhead, &vLookAhead, (rTuning.m_fLookAheadDist / fLen));
    };

    if (bRelocated)
    {
        m_vLookAhead = Math::VECTOR3_ZERO;
    }
    else
    {
        float t = Clamp((1.0f - std::exp(-rTuning.m_fLookAheadSpeed * dt)), 0.0f, 1.0f);
        Math::Vec3_Lerp(&m_vLookAhead, &m_vLookAhead, &vLookAhead, t);
    };

    Math::Vec3_Add(pvCenter, pvCenter, &m_vLookAhead);

    return numTarget;
};


#ifdef _DEBUG
static void ShowCameraDiag(CMapCamera* pMapCamera, const CMapCamera::FRAMETARGET* aTarget, int32 numTarget)
{
    if (!CGameStageDebug::CAMERA_DIAG)
        return;

    const CMapCamera::FRAMEDIAG& rDiag = pMapCamera->GetFrameDiag();

    RwV3d vLookat = Math::VECTOR3_ZERO;
    pMapCamera->GetLookat(&vLookat);

    char szBuff[128];
    szBuff[0] = '\0';

    CDebugShape::m_fLabelHeight = 14.0f;
    CDebugShape::m_fLabelOffsetY = 0.0f;

    std::sprintf(szBuff, "cam: %s area: %.24s",
                 (rDiag.m_bFraming ? "ENHANCED" : "ORIGINAL"),
                 pMapCamera->GetCameraAreaName());
    CDebugShape::ShowLabel(&vLookat, szBuff);
    CDebugShape::m_fLabelOffsetY += CDebugShape::m_fLabelHeight;

    std::sprintf(szBuff, "players: %d targets: %d outside: %d",
                 static_cast<int>(CGameProperty::GetPlayerNum()),
                 static_cast<int>(rDiag.m_bFraming ? rDiag.m_nNumTarget : 0),
                 static_cast<int>(rDiag.m_bFraming ? rDiag.m_nNumOutside : 0));
    CDebugShape::ShowLabel(&vLookat, szBuff);
    CDebugShape::m_fLabelOffsetY += CDebugShape::m_fLabelHeight;

    if (rDiag.m_bFraming)
    {
        std::sprintf(szBuff, "zoom: %.2f (req %.2f) fov: %.2f",
                     rDiag.m_fZoom, rDiag.m_fZoomRequest, rDiag.m_fFovZoom);
        CDebugShape::ShowLabel(&vLookat, szBuff);
        CDebugShape::m_fLabelOffsetY += CDebugShape::m_fLabelHeight;
    };

    std::sprintf(szBuff, "at: %.1f %.1f %.1f move radius: %.1f",
                 vLookat.x, vLookat.y, vLookat.z, rDiag.m_fMoveRadius);
    CDebugShape::ShowLabel(&vLookat, szBuff);

    CDebugShape::m_fLabelOffsetY = 0.0f;

    CDebugShape::ShowSphere(&vLookat, 0.15f, { 0xFF, 0xFF, 0x00, 0xFF });

    if (!rDiag.m_bFraming)
        return;

    for (int32 i = 0; (i < numTarget) && (i < rDiag.m_nNumTarget); ++i)
    {
        float fMargin = (aTarget[i].m_bPrimary ? CMapCamera::m_tuning.m_fSafeMargin
                                               : CMapCamera::m_tuning.m_fSecondaryMargin);

        bool bOutside = ((std::fabs(rDiag.m_afScreenX[i]) > fMargin) ||
                         (std::fabs(rDiag.m_afScreenY[i]) > fMargin));

        std::sprintf(szBuff, "%s %+.2f %+.2f",
                     (aTarget[i].m_bPrimary ? "P" : "B"),
                     rDiag.m_afScreenX[i],
                     rDiag.m_afScreenY[i]);

        RwV3d vPos = aTarget[i].m_vPos;
        vPos.y += aTarget[i].m_fUp;

        if (bOutside)
            CDebugShape::ShowLabel(&vPos, szBuff, { 0xFF, 0x40, 0x40, 0xFF });
        else
            CDebugShape::ShowLabel(&vPos, szBuff, { 0x40, 0xFF, 0x40, 0xFF });
    };
};
#endif /* _DEBUG */


//
// *********************************************************************************
//


/*static*/ CDefaultCameraUpdater* CDefaultCameraUpdater::Instance(void)
{
    static CDefaultCameraUpdater s_DefaultCameraUpdt;
    return &s_DefaultCameraUpdt;
};


void CDefaultCameraUpdater::Update(CMapCamera* pMapCamera)
{
    ASSERT(pMapCamera);

    if (CMapCamera::IsEnhanced())
        UpdateEnhanced(pMapCamera);
    else
        UpdateOriginal(pMapCamera);
};


void CDefaultCameraUpdater::UpdateOriginal(CMapCamera* pMapCamera)
{
    RwV3d avPosition[GAMETYPES::PLAYERS_MAX] = { Math::VECTOR3_ZERO };
    RwV3d vLookat = Math::VECTOR3_ZERO;

    int32 playerNum = CGameProperty::GetPlayerNum();
    for (int32 i = 0; i < playerNum; ++i)
    {
        RwV3d vTmp = Math::VECTOR3_ZERO;
        CGameProperty::Player(i)->GetPosition(&vTmp);

        avPosition[i] = vTmp;
        
        vLookat.x += vTmp.x;
        vLookat.y += vTmp.y;
        vLookat.z += vTmp.z;
    };

    if (playerNum > 0)
    {
        vLookat.x *= (1.0f / static_cast<float>(playerNum));
        vLookat.y *= (1.0f / static_cast<float>(playerNum));
        vLookat.z *= (1.0f / static_cast<float>(playerNum));
    };

    float fZoom = pMapCamera->CalcNiceZoom(avPosition, playerNum);
#if defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS)
    fZoom *= CGameStageDebug::CAMERA_ZOOM_SCALE;
#endif /* defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS) */
    pMapCamera->Update(&vLookat, fZoom);

#ifdef _DEBUG
    ShowCameraDiag(pMapCamera, nullptr, 0);
#endif /* _DEBUG */
};


void CDefaultCameraUpdater::UpdateEnhanced(CMapCamera* pMapCamera)
{
    CMapCamera::FRAMETARGET aTarget[CMapCamera::FRAMETARGET_MAX];
    RwV3d vLookat = Math::VECTOR3_ZERO;

    int32 numTarget = CPlayerFrameTracker::Instance().Update(aTarget, &vLookat);

    /* nobody to look at (all players are gone) - stay where camera is */
    if (!numTarget)
        pMapCamera->GetLookat(&vLookat);

    float fZoomScale = 1.0f;
#if defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS)
    fZoomScale *= CGameStageDebug::CAMERA_ZOOM_SCALE;
#endif /* defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS) */
    pMapCamera->UpdateFraming(&vLookat, aTarget, numTarget, fZoomScale);

#ifdef _DEBUG
    ShowCameraDiag(pMapCamera, aTarget, numTarget);
#endif /* _DEBUG */
};


//
// *********************************************************************************
//


/*static*/ CBossBattleCameraUpdater* CBossBattleCameraUpdater::Instance(void)
{
    static CBossBattleCameraUpdater s_BossBattleCameraUpdt;
    return &s_BossBattleCameraUpdt;
};


void CBossBattleCameraUpdater::Update(CMapCamera* pMapCamera)
{
    ASSERT(pMapCamera);

    if (CMapCamera::IsEnhanced())
        UpdateEnhanced(pMapCamera);
    else
        UpdateOriginal(pMapCamera);
};


void CBossBattleCameraUpdater::UpdateOriginal(CMapCamera* pMapCamera)
{
    RwV3d vLookat = Math::VECTOR3_ZERO;
    int32 enemyNum = 0;

    int32 playerNum = CGameProperty::GetPlayerNum();
    for (int32 i = 0; i < playerNum; ++i)
    {
        RwV3d vTmp = Math::VECTOR3_ZERO;
        CGameProperty::Player(i)->GetPosition(&vTmp);
        
        vLookat.x += vTmp.x;
        vLookat.y += vTmp.y;
        vLookat.z += vTmp.z;
    };    

    CGameObject* pGameObject = CGameObjectManager::GetNext(GAMEOBJECTTYPE::CHARACTER);
    while (pGameObject)
    {
        ASSERT(pGameObject->GetType() == GAMEOBJECTTYPE::CHARACTER);
        CCharacter* pChr = static_cast<CCharacter*>(pGameObject);

        if (pChr->GetAttackCharacterType() == CCharacter::TYPE_ENEMY)
        {
            CCharacterCompositor* pChrCompositor = static_cast<CCharacterCompositor*>(pChr);

            if ((pChrCompositor->GetID() >= ENEMYID::BOSSBEGIN) &&
                (pChrCompositor->GetID() <  ENEMYID::BOSSEND))
            {
                ++enemyNum;

                RwV3d vTmp = Math::VECTOR3_ZERO;
                pChrCompositor->GetPosition(&vTmp);

                vLookat.x += vTmp.x;
                vLookat.y += vTmp.y;
                vLookat.z += vTmp.z;
            };
        };

        pGameObject = CGameObjectManager::GetNext(GAMEOBJECTTYPE::CHARACTER, pGameObject);
    };

    int32 totalNum = (playerNum + enemyNum);
    if (totalNum > 0)
    {
        vLookat.x *= (1.0f / static_cast<float>(totalNum));
        vLookat.y *= (1.0f / static_cast<float>(totalNum));
        vLookat.z *= (1.0f / static_cast<float>(totalNum));
    };

    pMapCamera->Update(&vLookat);

#ifdef _DEBUG
    ShowCameraDiag(pMapCamera, nullptr, 0);
#endif /* _DEBUG */
};


/*
 *  Players are primary targets: lookat stays near them and they are always framed.
 *  Bosses are secondary: they pull lookat towards themselves (limited by weight and
 *  range, so boss that is huge or far away is not able to drag camera off the players)
 *  and they are framed with whatever zoom is left.
 */
void CBossBattleCameraUpdater::UpdateEnhanced(CMapCamera* pMapCamera)
{
    /* sanity limits for collision height of bosses */
    const float fBossHeightMin = 1.5f;
    const float fBossHeightMax = 4.0f;

    const CMapCamera::TUNING& rTuning = CMapCamera::m_tuning;

    CMapCamera::FRAMETARGET aTarget[CMapCamera::FRAMETARGET_MAX];
    RwV3d vLookat = Math::VECTOR3_ZERO;

    int32 numPlayer = CPlayerFrameTracker::Instance().Update(aTarget, &vLookat);
    int32 numTarget = numPlayer;

    RwV3d vBossCenter = Math::VECTOR3_ZERO;
    int32 numBoss = 0;

    CGameObject* pGameObject = CGameObjectManager::GetNext(GAMEOBJECTTYPE::CHARACTER);
    while (pGameObject)
    {
        ASSERT(pGameObject->GetType() == GAMEOBJECTTYPE::CHARACTER);
        CCharacter* pChr = static_cast<CCharacter*>(pGameObject);

        if (pChr->GetAttackCharacterType() == CCharacter::TYPE_ENEMY)
        {
            CCharacterCompositor* pChrCompositor = static_cast<CCharacterCompositor*>(pChr);

            if ((pChrCompositor->GetID() >= ENEMYID::BOSSBEGIN) &&
                (pChrCompositor->GetID() <  ENEMYID::BOSSEND))
            {
                RwV3d vTmp = Math::VECTOR3_ZERO;
                pChrCompositor->GetPosition(&vTmp);

                vBossCenter.x += vTmp.x;
                vBossCenter.y += vTmp.y;
                vBossCenter.z += vTmp.z;
                ++numBoss;

                if (numTarget < CMapCamera::FRAMETARGET_MAX)
                {
                    float fHeight = pChrCompositor->GetCollisionParameter().m_fHeight;

                    aTarget[numTarget].m_vPos     = vTmp;
                    aTarget[numTarget].m_fUp      = Clamp(fHeight, fBossHeightMin, fBossHeightMax);
                    aTarget[numTarget].m_fDown    = 0.0f;
                    aTarget[numTarget].m_bPrimary = false;
                    ++numTarget;
                };
            };
        };

        pGameObject = CGameObjectManager::GetNext(GAMEOBJECTTYPE::CHARACTER, pGameObject);
    };

    if (numBoss > 0)
    {
        Math::Vec3_Scale(&vBossCenter, &vBossCenter, (1.0f / static_cast<float>(numBoss)));

        if (numPlayer > 0)
        {
            /* same share as in original average, but limited */
            float fWeight = (static_cast<float>(numBoss) / static_cast<float>(numPlayer + numBoss));
            fWeight = Clamp(fWeight, 0.0f, rTuning.m_fBossWeightMax);

            RwV3d vToBoss = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&vToBoss, &vBossCenter, &vLookat);

            float fDist = Math::Vec3_Length(&vToBoss);
            if ((fDist > rTuning.m_fBossRange) && (fDist > 0.0f))
                Math::Vec3_Scale(&vToBoss, &vToBoss, (rTuning.m_fBossRange / fDist));

            Math::Vec3_Scale(&vToBoss, &vToBoss, fWeight);
            Math::Vec3_Add(&vLookat, &vLookat, &vToBoss);
        }
        else
        {
            vLookat = vBossCenter;
        };
    }
    else if (!numPlayer)
    {
        pMapCamera->GetLookat(&vLookat);
    };

    float fZoomScale = 1.0f;
#if defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS)
    fZoomScale *= CGameStageDebug::CAMERA_ZOOM_SCALE;
#endif /* defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS) */
    pMapCamera->UpdateFraming(&vLookat, aTarget, numTarget, fZoomScale);

#ifdef _DEBUG
    ShowCameraDiag(pMapCamera, aTarget, numTarget);
#endif /* _DEBUG */
};
