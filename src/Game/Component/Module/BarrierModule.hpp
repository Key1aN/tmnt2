#pragma once

#include "Module.hpp"

#include "Game/System/Hit/HitAttackData.hpp"


class CSphere;
class CTracer;
class CGameObject;


class CBarrierModule : public IModule
{
public:
    static const char* BARRIER_MOTIONNAME;
    
    enum STATE
    {
        STATE_APPEAR = 0,
        STATE_IDLE,
        STATE_DISAPPEAR,
        STATE_SLEEP,
    };
    
public:
    static void DrawAll(void);
    
    CBarrierModule(CTracer* pTracer, CGameObject* pObject, float fRadius);
    virtual ~CBarrierModule(void);
    virtual void Run(void) override;
    virtual void Draw(void) override;
    void DrawBarrier(void);
    void SetLive(float fLivetime);
    void SetHitTarget(CHitAttackData::TARGET target);
    void SetStepOne(void);
    void SetStepTwo(void);
    void SetStepThree(void);
    void Appear(void);
    void Disappear(void);
    STATE GetState(void) const;
#if defined(TMNT2_DEBUG_TOOLS)
    // Combat Overhaul 2 (Michelangelo's guard shield): same sphere, texture
    // and animation in another colour; it only reflects shots, so it neither
    // hits enemies nor swallows projectiles.
    void SetShotReflectOnly(const RwRGBA& rColor);
    bool IsShotReflectOnly(void) const { return m_bShotReflectOnly; };
#endif /* defined(TMNT2_DEBUG_TOOLS) */

protected:
    static CList<CBarrierModule> m_listDraw;
    CListNode<CBarrierModule> m_nodeDraw;
    CSphere* m_pBarrierSphere;
    CGameObject* m_pDummy;
    CTracer* m_pTracer;
    CGameObject* m_pObject;
    RwTexture* m_pTexture;
    STATE m_state;
    float m_fRadius;
    float m_fScale;
    bool m_bDisplay;
    CHitAttackData::TARGET m_target;
    int32 m_nPower;
    float m_fTime;
    float m_fLivetime;
#if defined(TMNT2_DEBUG_TOOLS)
    bool m_bShotReflectOnly;
#endif /* defined(TMNT2_DEBUG_TOOLS) */
};